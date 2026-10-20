#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <ForgePact/Mark1Set.hpp>

namespace ForgePact::Mark1Drops {

// ---- Mark 1 drops: the drops' decision core (ForgePact #166, phase 3) -----------
//
// While the "Mark 1 drops" switch is on, each of the four Mark 1 pieces joins
// the game's own Heroic roll (`DropItemHeroic`) with one entry's share, the way
// Gabriel Wings join the Angelic roll (#74's stand-in, coin and rewrite
// machinery, FlightCharms.hpp's drops section):
//   - for the length of one call, one stand-in entry per piece is pushed onto
//     the sub-list that call reads: a triple of the piece's own class that the
//     roll keeps, chosen from the live list (no game reading says which Heroic
//     uniques the list holds, so there is no fixed table);
//   - a hit on a pushed stand-in's whole triple is the piece's with
//     probability c / (n + c), n the list's own entries carrying that triple
//     and c the copies pushed, so the stand-in keeps its own rate;
//   - the piece's hit is rewritten to its own definition (Mark1Set.hpp's
//     kPieces: a = its seed, b = its base), the game builds that base item and
//     the Mark 1 dressing makes it the piece.
//
// What the roll does is a static reading (the hub's docs/RUNTIME_DATA_MODELS.md
// § 23 and docs/models/unique-pick-spec.md, modelled in hs-game-sdk's
// unique_pick_model): it reads lootListUnique[k], k its third argument and 5
// when that is undefined, and draws again until it keeps an entry that is not
// flagged hidden and whose rarity is neither 7 (Angelic) nor 10 (Unholy). The
// placement takes the item's class from the entry, so a stand-in shares the
// piece's class.
//
// This header decides; it never touches the game. Numbers and strings only,
// never an instance or an RValue (the Mark1Set.hpp contract), so
// tests/mark1_drops_harness.cpp compiles it whole. The adapter in ModuleMain.cpp
// hooks the roll by name, reads the list and each triple's repository verdict
// by name outside a roll, pushes and removes the entries, and hands the
// numbers in here.

using Mark1Set::Piece;
using Mark1Set::PieceRow;
using Mark1Set::kPieceCount;
using Mark1Set::kPieces;

// ---- the constants the adapter and the hub's parity test read --------------------

// lootListUnique holds six sub-lists, [0] to [5] (RDM § 13.4).
inline constexpr int kListCount = 6;
// The sub-list the Heroic roll reads when its third argument is undefined
// (static reading; the model's HEROIC_DEFAULT_LIST_INDEX).
inline constexpr int kHeroicDefaultListIndex = 5;
// The copies per piece the player build pushes: g_Mark1DropCopies is a
// constexpr of this under FORGEPACT_RELEASE, one entry's share per piece.
inline constexpr int kPlayerCopies = 1;
// The research build's `mark1probe copies <k>` ceiling.
inline constexpr int kMaxCopies = 400;
// Heroic, the rarity a stand-in is preferred at (itemInfoStruct "27").
inline constexpr int kHeroicRarity = 9;
// The rarities the Heroic roll refuses and draws again on: 7 Angelic, 10
// Unholy (static reading; the model's HEROIC_REFUSED_RARITIES).
inline constexpr int kRefusedRarityCount = 2;
inline constexpr int kRefusedRarities[kRefusedRarityCount] = { 7, 10 };

// ---- which sub-list a call reads ---------------------------------------------------

// The kind of the call's third argument as the adapter read it: not passed,
// undefined, a number (real, int32 or int64), or anything else.
enum class ArgKind : int { Absent, Undefined, Number, Other };

inline constexpr int kListRefused = -1;

// The sub-list a call reads: 5 when the argument is absent or undefined, the
// argument itself when it is a whole number 0 to 5, and kListRefused for
// anything else (a fraction, out of range, not finite, another kind). A refused
// call pushes nothing and is counted (` list-refused=`).
inline int ListIndexOf(ArgKind kind, double value)
{
    if (kind == ArgKind::Absent || kind == ArgKind::Undefined) return kHeroicDefaultListIndex;
    if (kind != ArgKind::Number) return kListRefused;
    long long k = 0;
    if (!Mark1Set::AsWhole(value, k) || k < 0 || k >= kListCount) return kListRefused;
    return static_cast<int>(k);
}

// ---- what the roll keeps, and the stand-ins ----------------------------------------

// Whether the Heroic roll keeps an entry once drawn: not hidden, and neither
// Angelic nor Unholy.
inline bool HeroicKeeps(int rarity, bool hidden)
{
    if (hidden) return false;
    for (int r : kRefusedRarities)
        if (rarity == r) return false;
    return true;
}

// The piece whose class is `itemType`: helmet 0, body armor 1, boots 2,
// gloves 4 (Mark1Set.hpp's kPieces).
inline bool PieceOfClass(long long itemType, Piece& out)
{
    for (const PieceRow& r : kPieces) {
        if (r.itemType != itemType) continue;
        out = r.piece;
        return true;
    }
    return false;
}

// A list entry's [type, sub, b], as whole numbers.
struct Triple {
    long long t = 0;
    long long sub = 0;
    long long b = 0;
};

inline bool operator<(const Triple& x, const Triple& y)
{
    return std::tie(x.t, x.sub, x.b) < std::tie(y.t, y.sub, y.b);
}
inline bool operator==(const Triple& x, const Triple& y)
{
    return x.t == y.t && x.sub == y.sub && x.b == y.b;
}

// One entry of a sub-list as the adapter read it: a three-element array of
// numbers (`triple`), or anything else, which the roll draws past.
struct ListEntry {
    bool   triple = false;
    double t = 0;
    double sub = 0;
    double b = 0;
};

// The repository's verdict on a triple, read by name outside a roll (the reads
// FlightDropUniquePasses makes): its rarity and whether it is flagged hidden.
// False when it could not be read; such a triple is counted and never a
// stand-in.
using VerdictFn = std::function<bool(const Triple& key, int& rarity, bool& hidden)>;

struct StandIn {
    bool   resolved = false;
    Triple triple;
    int    rarity = 0;
    int    n = 0;  // entries of the list carrying this triple, repeats counted
};

// Per piece class: the distinct triples of that class the roll keeps, and the
// entries carrying them (repeats counted). `mark1probe pool` prints both.
struct ClassTally {
    int keptTriples = 0;
    int keptEntries = 0;
};

struct Resolution {
    int index = kListRefused;
    int length = 0;   // the list's entries, every kind
    int unread = 0;   // distinct triples of a piece class whose verdict could not be read
    std::array<StandIn, kPieceCount>    standIns{};
    std::array<ClassTally, kPieceCount> kept{};

    bool AnyResolved() const
    {
        for (const StandIn& s : standIns)
            if (s.resolved) return true;
        return false;
    }
};

// The stand-in rule. For each piece's class: a triple of that class the roll
// keeps, Heroic (9) preferred, then any kept rarity, ties broken by the lowest
// (sub, b). Only triples of a piece class are asked about, each distinct one
// once. A class with no such triple has no stand-in on this list: that piece
// does not drop from it, and the others still do.
inline Resolution ResolveStandIns(int index, const std::vector<ListEntry>& list, const VerdictFn& verdict)
{
    Resolution r;
    r.index = index;
    r.length = static_cast<int>(list.size());
    std::map<Triple, int> counts;  // ordered by (type, sub, b): the lowest (sub, b) of a class first
    for (const ListEntry& e : list) {
        if (!e.triple) continue;
        Triple k;
        Piece p = Piece::Helmet;
        if (!Mark1Set::AsWhole(e.t, k.t) || !Mark1Set::AsWhole(e.sub, k.sub) || !Mark1Set::AsWhole(e.b, k.b)) continue;
        if (!PieceOfClass(k.t, p)) continue;
        ++counts[k];
    }
    for (const auto& [key, n] : counts) {
        Piece p = Piece::Helmet;
        PieceOfClass(key.t, p);
        int rarity = 0;
        bool hidden = false;
        if (!verdict || !verdict(key, rarity, hidden)) {
            ++r.unread;
            continue;
        }
        if (!HeroicKeeps(rarity, hidden)) continue;
        const size_t i = static_cast<size_t>(p);
        ++r.kept[i].keptTriples;
        r.kept[i].keptEntries += n;
        StandIn& s = r.standIns[i];
        if (!s.resolved || (rarity == kHeroicRarity && s.rarity != kHeroicRarity)) {
            s.resolved = true;
            s.triple = key;
            s.rarity = rarity;
            s.n = n;
        }
    }
    return r;
}

// The stand-ins per sub-list, resolved again when the list the call reads
// changed: another array (its id) or another length.
class StandInCache {
public:
    bool Stale(int index, uint64_t listId, int length) const
    {
        if (index < 0 || index >= kListCount) return true;
        const Slot& s = slots_[static_cast<size_t>(index)];
        return !s.have || s.id != listId || s.length != length;
    }
    void Store(uint64_t listId, const Resolution& r)
    {
        if (r.index < 0 || r.index >= kListCount) return;
        Slot& s = slots_[static_cast<size_t>(r.index)];
        s.have = true;
        s.id = listId;
        s.length = r.length;
        s.resolution = r;
    }
    const Resolution* Find(int index) const
    {
        if (index < 0 || index >= kListCount) return nullptr;
        const Slot& s = slots_[static_cast<size_t>(index)];
        return s.have ? &s.resolution : nullptr;
    }

private:
    struct Slot {
        bool       have = false;
        uint64_t   id = 0;
        int        length = 0;
        Resolution resolution;
    };
    std::array<Slot, kListCount> slots_{};
};

// ---- the push, the coin, the hit and the rewrite -----------------------------------

struct PushEntry {
    Piece  piece = Piece::Helmet;
    Triple triple;
    int    n = 0;       // the list's own entries carrying the triple
    int    copies = 0;  // entries pushed for it
};

// What one call pushes: `copies` entries per piece that has a stand-in on the
// list and is not latched off. Copies below 1 push nothing; above kMaxCopies
// push kMaxCopies.
struct PushPlan {
    int                    index = kListRefused;
    std::vector<PushEntry> entries;

    int EntryCount() const
    {
        int total = 0;
        for (const PushEntry& e : entries) total += e.copies;
        return total;
    }
};

inline PushPlan PlanPush(const Resolution& r, int copies, const std::array<bool, kPieceCount>& latched)
{
    PushPlan plan;
    plan.index = r.index;
    if (copies < 1) return plan;
    if (copies > kMaxCopies) copies = kMaxCopies;
    for (int i = 0; i < kPieceCount; ++i) {
        const StandIn& s = r.standIns[static_cast<size_t>(i)];
        if (!s.resolved || latched[static_cast<size_t>(i)]) continue;
        plan.entries.push_back(PushEntry{ kPieces[i].piece, s.triple, s.n, copies });
    }
    return plan;
}

// The share of a stand-in hit that becomes the piece: c / (n + c).
inline double CoinShare(long long n, long long c)
{
    if (n < 0 || c <= 0) return 0.0;
    return static_cast<double>(c) / static_cast<double>(n + c);
}

// Whether a hit is ours, for u drawn uniformly from [0, 1).
inline bool CoinOurs(long long n, long long c, double u)
{
    if (!(u >= 0.0 && u < 1.0)) return false;
    return u < CoinShare(n, c);
}

// A typed hit during a call that pushed `plan`: the piece when its whole
// triple is a pushed stand-in's and the coin falls ours; otherwise the hit
// stays the game's own. Nothing pushed attributes nothing.
inline bool AttributeHit(const PushPlan& plan, double t, double sub, double b, double u, Piece& out)
{
    Triple k;
    if (!Mark1Set::AsWhole(t, k.t) || !Mark1Set::AsWhole(sub, k.sub) || !Mark1Set::AsWhole(b, k.b)) return false;
    for (const PushEntry& e : plan.entries) {
        if (!(e.triple == k)) continue;
        if (!CoinOurs(e.n, e.copies, u)) return false;
        out = e.piece;
        return true;
    }
    return false;
}

// What the piece's hit is rewritten to through SignatureRewriteParams: its own
// definition, a = its seed, b = its base, c 0 and j 0.
struct RewriteTarget {
    int       itemType = 0;
    long long a = 0;
    int       b = 0;
    int       c = 0;
    int       j = 0;
};

inline RewriteTarget RewriteFor(Piece p)
{
    const PieceRow& r = Mark1Set::RowOf(p);
    return RewriteTarget{ r.itemType, r.seed, r.baseB, 0, 0 };
}

// ---- the art: no roll builds a piece before the ground install was tried --------
//
// A piece built on the ground while the shared LootGroundInit route is still
// `not-installed` draws Error_spr (RDM § 23.7). So while the art is armed with
// an icon embedded, a call pushes nothing until the ground consumer's one
// install was tried (` art-wait=`); the drops install tries it first when it is
// pending.
inline bool ArtReady(bool armed, int iconsEmbedded, bool groundTried)
{
    return !armed || iconsEmbedded <= 0 || groundTried;
}

// ---- the gate ---------------------------------------------------------------------

// The ` gate=` field, as the flight charms' is.
enum class Gate : int { Off, On, Refused, NotResolved, NoDetection };

inline constexpr std::string_view GateName(Gate g)
{
    switch (g) {
    case Gate::Off:         return "off";
    case Gate::On:          return "on";
    case Gate::Refused:     return "refused";
    case Gate::NotResolved: return "not-resolved";
    case Gate::NoDetection: return "no-detection";
    }
    return "?";
}

// What one call does, decided before anything is read from the list.
enum class CallAction : int {
    PassOff,          // the switch is off (or the hook is not in): the game's own roll
    PassGate,         // #74's detection is not all inline: the game's own roll
    PassNested,       // a roll already in progress: the game's own roll, counted
    PassArtWait,      // the art is not ready: the game's own roll, counted
    PassListRefused,  // the third argument names no sub-list: the game's own roll, counted
    Resolve,          // read the list, re-resolve if stale, push the plan
};

// ---- the `mark1set drops:` line ---------------------------------------------------------

// ` <key>=<ours>/<built>`: stand-in hits that became the piece, of those the
// Custom Forge's final pass counted built.
struct DropTally {
    uint64_t ours = 0;
    uint64_t built = 0;
};

// Every field of the line. A default DropStatFields prints the fresh
// session's line; Drops::FillStat fills it but the hook route, which the
// adapter sets.
struct DropStatFields {
    bool        drops = false;
    std::string install = "not-armed";
    Gate        gate = Gate::Off;
    std::string hook = "not-installed";  // the DropItemHeroic route: not-installed, both or refused
    int         list = kListRefused;     // the sub-list the last pushed-for call read; `-` before one
    uint64_t    heroicCalls = 0;         // every call the detour saw, switch on or off
    uint64_t    pushedCalls = 0;
    uint64_t    nested = 0;
    uint64_t    artWait = 0;
    DropTally   pieces[kPieceCount];
    uint64_t    injected = 0;            // entries pushed, all calls
    uint64_t    refused = 0;             // rewrites refused (that piece latched off)
    uint64_t    anomalies = 0;           // removals that did not find the list as pushed
    std::array<StandIn, kPieceCount> standIns{};
    uint64_t    listRefused = 0;         // calls whose third argument named no sub-list
    int         unread = 0;              // the last resolution's unreadable verdicts
};

// A piece's stand-in as the line prints it: `<sub>:<b>@<rarity>x<n>`, or none.
inline std::string StandInText(const StandIn& s)
{
    if (!s.resolved) return "none";
    return std::to_string(s.triple.sub) + ":" + std::to_string(s.triple.b) + "@" + std::to_string(s.rarity)
        + "x" + std::to_string(s.n);
}

// `mark1set stat`'s second line, after the set's own. It begins
// `mark1set drops:`; a field added later goes at the end.
inline std::string DropStatLine(const DropStatFields& f)
{
    std::string s = "mark1set drops: drops=" + std::string(f.drops ? "on" : "off");
    s += " install=" + f.install;
    s += " gate=" + std::string(GateName(f.gate));
    s += " hook=" + f.hook;
    s += " list=" + (f.list == kListRefused ? std::string("-") : std::to_string(f.list));
    s += " heroic-calls=" + std::to_string(f.heroicCalls);
    s += " pushed-calls=" + std::to_string(f.pushedCalls);
    s += " nested=" + std::to_string(f.nested);
    s += " art-wait=" + std::to_string(f.artWait);
    for (int i = 0; i < kPieceCount; ++i)
        s += " " + std::string(kPieces[i].key) + "=" + std::to_string(f.pieces[i].ours) + "/" + std::to_string(f.pieces[i].built);
    s += " injected=" + std::to_string(f.injected);
    s += " refused=" + std::to_string(f.refused);
    s += " anomalies=" + std::to_string(f.anomalies);
    for (int i = 0; i < kPieceCount; ++i)
        s += " standin-" + std::string(kPieces[i].key) + "=" + StandInText(f.standIns[static_cast<size_t>(i)]);
    s += " list-refused=" + std::to_string(f.listRefused);
    s += " unread=" + std::to_string(f.unread);
    return s;
}

// ---- the drops: the switch, the install, the gate and the counters -------------

class Drops {
public:
    // The `mark1set drops 1|0` switch. A hook that went in stays in and passes
    // through while it is off. SetSwitch below also arms the pieces' art.
    void SetEnabled(bool on) { enabled_ = on; }
    bool Enabled() const { return enabled_; }

    // ---- the install, as Mark1Set::Bonus's (the guide's Known Limitations
    // item 8): `mark1set drops 1` with no character only arms; the Mark 1
    // tick installs once setup holds and the local player resolves; with a
    // character it installs at once. A refusal turns the switch off.
    static constexpr unsigned long long kInstallPollFrames = Mark1Set::Bonus::kInstallPollFrames;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return Mark1Set::Bonus::LooksForPlayer(armedFrames);
    }
    bool TickNeeded() const { return enabled_ && !installed_ && !tickTried_; }
    bool ShouldInstall(bool setupDone, bool playerResolved, bool fromTick) const
    {
        return enabled_ && !installed_ && !(fromTick && tickTried_) && setupDone && playerResolved;
    }
    void NoteInstall(bool ok, bool fromTick)
    {
        if (fromTick) tickTried_ = true;
        if (ok) {
            installed_ = true;
            refused_ = false;
            return;
        }
        refused_ = true;
        enabled_ = false;
    }
    bool Installed() const { return installed_; }
    std::string_view InstallStateName() const
    {
        if (installed_) return "installed";
        if (refused_) return "refused";
        return enabled_ ? "waiting-for-character" : "not-armed";
    }

    // The ` gate=` field. `detectionNative`: #74's detection
    // (InstallSignatureAngelicHooks) is all four inline.
    Gate GateNow(bool detectionNative) const
    {
        if (refused_) return Gate::Refused;
        if (!enabled_ || !installed_) return Gate::Off;
        if (!detectionNative) return Gate::NoDetection;
        if (listState_ == ListState::NoneResolved || listState_ == ListState::Unreadable) return Gate::NotResolved;
        return Gate::On;
    }

    // ---- per call: counted, then decided before the list is read -------------
    // `rollDepth` is g_SigRollDepth; `artReady` is ArtReady(...). On Resolve,
    // `listIndex` is the sub-list the call reads.
    CallAction BeginCall(bool detectionNative, int rollDepth, bool artReady, ArgKind kind, double value, int& listIndex)
    {
        ++heroicCalls_;
        listIndex = kListRefused;
        if (!enabled_ || !installed_) return CallAction::PassOff;
        if (!detectionNative) return CallAction::PassGate;
        if (rollDepth > 0) {
            ++nested_;
            return CallAction::PassNested;
        }
        if (!artReady) {
            ++artWait_;
            return CallAction::PassArtWait;
        }
        listIndex = ListIndexOf(kind, value);
        if (listIndex == kListRefused) {
            ++listRefused_;
            return CallAction::PassListRefused;
        }
        return CallAction::Resolve;
    }

    StandInCache&       Cache() { return cache_; }
    const StandInCache& Cache() const { return cache_; }

    // The list could not be read: nothing pushed, the gate reads not-resolved.
    void NoteListUnreadable(int index)
    {
        lastList_ = index;
        lastUnread_ = 0;
        listState_ = ListState::Unreadable;
    }

    // The plan for a call whose list resolved to `r`; it records the
    // resolution the line prints.
    PushPlan PlanFor(const Resolution& r, int copies)
    {
        lastList_ = r.index;
        lastStandIns_ = r.standIns;
        lastUnread_ = r.unread;
        listState_ = r.AnyResolved() ? ListState::Resolved : ListState::NoneResolved;
        return PlanPush(r, copies, latched_);
    }

    // The plan's entries went onto the list and were held read-back.
    void NotePushed(const PushPlan& p)
    {
        if (p.entries.empty()) return;
        ++pushedCalls_;
        injected_ += static_cast<uint64_t>(p.EntryCount());
    }
    // A removal that did not find the list as it pushed it.
    void NoteAnomaly() { ++anomalies_; }

    void NoteOurs(Piece p) { ++tally_[static_cast<size_t>(p)].ours; }
    void NoteBuilt(Piece p) { ++tally_[static_cast<size_t>(p)].built; }
    // A refused rewrite latches that piece off for the session, as #74 does.
    void NoteRewriteRefused(Piece p)
    {
        ++rewriteRefused_;
        latched_[static_cast<size_t>(p)] = true;
    }
    bool Latched(Piece p) const { return latched_[static_cast<size_t>(p)]; }

    uint64_t HeroicCalls() const { return heroicCalls_; }
    uint64_t PushedCalls() const { return pushedCalls_; }

    // Every field but `hook=`, which the adapter sets.
    void FillStat(DropStatFields& f, bool detectionNative) const
    {
        f.drops = enabled_;
        f.install = std::string(InstallStateName());
        f.gate = GateNow(detectionNative);
        f.list = lastList_;
        f.heroicCalls = heroicCalls_;
        f.pushedCalls = pushedCalls_;
        f.nested = nested_;
        f.artWait = artWait_;
        for (int i = 0; i < kPieceCount; ++i) f.pieces[i] = tally_[static_cast<size_t>(i)];
        f.injected = injected_;
        f.refused = rewriteRefused_;
        f.anomalies = anomalies_;
        f.standIns = lastStandIns_;
        f.listRefused = listRefused_;
        f.unread = lastUnread_;
    }

private:
    enum class ListState : int { Unknown, Resolved, NoneResolved, Unreadable };

    bool         enabled_ = false;
    bool         installed_ = false;
    bool         refused_ = false;
    bool         tickTried_ = false;
    ListState    listState_ = ListState::Unknown;
    int          lastList_ = kListRefused;
    int          lastUnread_ = 0;
    std::array<StandIn, kPieceCount>   lastStandIns_{};
    std::array<bool, kPieceCount>      latched_{};
    std::array<DropTally, kPieceCount> tally_{};
    StandInCache cache_;
    uint64_t     heroicCalls_ = 0;
    uint64_t     pushedCalls_ = 0;
    uint64_t     nested_ = 0;
    uint64_t     artWait_ = 0;
    uint64_t     listRefused_ = 0;
    uint64_t     injected_ = 0;
    uint64_t     rewriteRefused_ = 0;
    uint64_t     anomalies_ = 0;
};

// `mark1set drops 1|0`: the drops switch, and on a switch-on the pieces' art,
// armed for the session as `mark1set bonus 1` arms it, so a dropped piece shows
// the owner's art. One flag, armed by either switch (Bonus::ArtArmed).
inline void SetSwitch(Drops& drops, Mark1Set::Bonus& set, bool on)
{
    drops.SetEnabled(on);
    set.NoteDropsSwitch(on);
}

// The reply to `mark1set drops 1|0`, after the adapter has tried its install.
inline std::string DropsReply(const Drops& d, bool detectionNative)
{
    return "mark1set: drops=" + std::string(d.Enabled() ? "on" : "off") + " install=" + std::string(d.InstallStateName())
        + " gate=" + std::string(GateName(d.GateNow(detectionNative)));
}

} // namespace ForgePact::Mark1Drops
