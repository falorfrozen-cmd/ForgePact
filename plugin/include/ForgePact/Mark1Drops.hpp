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
// The kill route (the owner's 2026-10-20 route, "The kill route" below) joins
// the pieces to the game's ordinary unique roll (`DropUniqueItems`, which a
// kill's drop runs) the same way, through the one detour Pocket Elf already
// holds on that roll: a stand-in of the piece's class that roll keeps, pushed
// onto each of the five sub-lists it reads, and the same coin and rewrite.
// Both rolls share the switch, the install, the art-ready rule, the latches
// and the `injected=`, `refused=`, `anomalies=` and `art-wait=` counters; each
// has its own gate, stand-ins and tallies.
//
// Live 3 then measured that ordinary kills never reach `DropUniqueItems`: a
// kill's unique comes from DropItem's own draw (Mod live 7 results). The kill
// draw ("the kill draw" below) joins the pieces to that draw the same way,
// through DropManager's one DropItem detour and raredrop's one
// LoadSatanicDropTier detour: a stand-in of the piece's class the draw keeps,
// pushed onto each of the six lists for the length of a DropItem call, the coin
// c / (n_k + c) from the list k the call's tier names, and the same rewrite. It
// shares the switch, the install, the art-ready rule, the latches and those
// counters too, with its own gate, stand-ins and tallies. The Heroic and
// DropUniqueItems routes stay as they are.
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

// A typed hit among the entries a call pushed: the piece when its whole
// triple is a pushed stand-in's and the coin falls ours; otherwise the hit
// stays the game's own. Nothing pushed attributes nothing.
inline bool AttributeHitAmong(const std::vector<PushEntry>& entries, double t, double sub, double b, double u, Piece& out)
{
    Triple k;
    if (!Mark1Set::AsWhole(t, k.t) || !Mark1Set::AsWhole(sub, k.sub) || !Mark1Set::AsWhole(b, k.b)) return false;
    for (const PushEntry& e : entries) {
        if (!(e.triple == k)) continue;
        if (!CoinOurs(e.n, e.copies, u)) return false;
        out = e.piece;
        return true;
    }
    return false;
}

// A typed hit during a Heroic roll that pushed `plan`.
inline bool AttributeHit(const PushPlan& plan, double t, double sub, double b, double u, Piece& out)
{
    return AttributeHitAmong(plan.entries, t, sub, b, u, out);
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

// ---- the kill route: the ordinary unique roll ---------------------------------------
//
// A kill reaches the ordinary unique roll (`DropUniqueItems`, through DropItem
// and LoadDrops: RDM § 13.1 and § 23.4, static readings). That roll picks one
// of lootListUnique[0..4] by a zone value, draws one entry, and keeps it only
// when its rarity is below 7 and it is not flagged hidden; a refused draw
// drops nothing. The Heroic roll's stand-ins (Heroic, 9) are refused there,
// so the kill route has stand-ins of its own, Satanic or below; the rewrite
// still makes a hit the piece. The roll cannot say which of the five lists it
// drew (RDM § 23.8), so a piece's entries go onto all five, and a stand-in
// qualifies only when the five hold it equally often: then the coin c / (n + c)
// is exact whichever list the call read (Pocket Elf's rule, FlightCharms.hpp).

// Which roll a hit or a built piece came from: its tallies are its own. Draw
// is DropItem's own unique draw ("the kill draw" below).
enum class Roll : int { Heroic, Kill, Draw };

// The sub-lists the ordinary roll reads (static reading; the model's
// UNIQUE_SUB_LISTS). The kill route pushes onto each of them.
inline constexpr int kKillListCount = 5;
inline constexpr int kKillLists[kKillListCount] = { 0, 1, 2, 3, 4 };
// The ordinary roll keeps an entry whose rarity is below this (static
// reading; the model's ORDINARY_RARITY_BELOW).
inline constexpr int kOrdinaryRarityBelow = 7;

// Whether the ordinary roll keeps the entry its one draw read: not hidden, and
// a rarity below 7.
inline bool OrdinaryKeeps(int rarity, bool hidden)
{
    if (hidden) return false;
    return rarity < kOrdinaryRarityBelow;
}

// All six sub-lists, [0] to [5], as the adapter read them. The candidates
// come from any of them; the counts that decide come from [0..4].
using KillLists = std::array<std::vector<ListEntry>, kListCount>;

struct KillResolution {
    std::array<int, kListCount>         lengths{};  // each list's entries, every kind
    int                                 unread = 0; // distinct triples of a piece class whose verdict could not be read
    std::array<StandIn, kPieceCount>    standIns{}; // n is the count on each of [0..4]
    // Per class: the distinct triples the ordinary roll keeps, and their
    // entries on [0..4] (repeats counted).
    std::array<ClassTally, kPieceCount> kept{};
    // Per class: of those, how many have the same count on all five.
    std::array<int, kPieceCount>        qualifying{};
    // Per class: the chosen stand-in's count on each of [0..4].
    std::array<std::array<int, kKillListCount>, kPieceCount> counts{};

    bool AnyResolved() const
    {
        for (const StandIn& s : standIns)
            if (s.resolved) return true;
        return false;
    }
};

// The kill route's stand-in rule. For each piece's class: the distinct triples
// of that class on any of [0..5], each verdict asked once; a triple qualifies
// when the ordinary roll keeps it and its count n is the same on each of
// [0..4]. The smallest n wins (n 0 first: then every hit on it is the
// piece's), ties by the lowest (sub, b). A class with none has no kill
// stand-in: that piece does not drop on kills, the others still do, and the
// Heroic route is untouched.
inline KillResolution ResolveKillStandIns(const KillLists& lists, const VerdictFn& verdict)
{
    KillResolution r;
    std::map<Triple, std::array<int, kKillListCount>> counts;  // ordered by (type, sub, b)
    for (int k = 0; k < kListCount; ++k) {
        const std::vector<ListEntry>& list = lists[static_cast<size_t>(k)];
        r.lengths[static_cast<size_t>(k)] = static_cast<int>(list.size());
        for (const ListEntry& e : list) {
            if (!e.triple) continue;
            Triple key;
            Piece p = Piece::Helmet;
            if (!Mark1Set::AsWhole(e.t, key.t) || !Mark1Set::AsWhole(e.sub, key.sub) || !Mark1Set::AsWhole(e.b, key.b)) continue;
            if (!PieceOfClass(key.t, p)) continue;
            std::array<int, kKillListCount>& c = counts[key];  // zeros when new
            if (k < kKillListCount) ++c[static_cast<size_t>(k)];
        }
    }
    for (const auto& [key, c] : counts) {
        Piece p = Piece::Helmet;
        PieceOfClass(key.t, p);
        int rarity = 0;
        bool hidden = false;
        if (!verdict || !verdict(key, rarity, hidden)) {
            ++r.unread;
            continue;
        }
        if (!OrdinaryKeeps(rarity, hidden)) continue;
        const size_t i = static_cast<size_t>(p);
        ++r.kept[i].keptTriples;
        bool equal = true;
        for (int n : c) {
            r.kept[i].keptEntries += n;
            if (n != c[0]) equal = false;
        }
        if (!equal) continue;
        ++r.qualifying[i];
        StandIn& s = r.standIns[i];
        if (!s.resolved || c[0] < s.n) {
            s.resolved = true;
            s.triple = key;
            s.rarity = rarity;
            s.n = c[0];
            r.counts[i] = c;
        }
    }
    return r;
}

// One list's identity as the adapter read it: its array and its length.
struct ListKey {
    uint64_t id = 0;
    int      length = 0;
};
using KillListKeys = std::array<ListKey, kListCount>;

// The kill route's stand-ins, resolved again when any of [0..5] changed:
// another array (its id) or another length.
class KillStandInCache {
public:
    bool Stale(const KillListKeys& keys) const
    {
        if (!have_) return true;
        for (size_t k = 0; k < keys.size(); ++k)
            if (keys[k].id != keys_[k].id || keys[k].length != keys_[k].length) return true;
        return false;
    }
    void Store(const KillListKeys& keys, const KillResolution& r)
    {
        have_ = true;
        keys_ = keys;
        resolution_ = r;
    }
    const KillResolution* Find() const { return have_ ? &resolution_ : nullptr; }

private:
    bool           have_ = false;
    KillListKeys   keys_{};
    KillResolution resolution_;
};

// What one kill-route call pushes: `copies` entries per piece that has a kill
// stand-in and is not latched off, onto each of kKillLists. Copies below 1
// push nothing; above kMaxCopies push kMaxCopies.
struct KillPushPlan {
    std::vector<PushEntry> entries;  // each one goes onto every list in kKillLists

    int PerListCount() const
    {
        int total = 0;
        for (const PushEntry& e : entries) total += e.copies;
        return total;
    }
    int EntryCount() const { return PerListCount() * kKillListCount; }
};

inline KillPushPlan PlanKillPush(const KillResolution& r, int copies, const std::array<bool, kPieceCount>& latched)
{
    KillPushPlan plan;
    if (copies < 1) return plan;
    if (copies > kMaxCopies) copies = kMaxCopies;
    for (int i = 0; i < kPieceCount; ++i) {
        const StandIn& s = r.standIns[static_cast<size_t>(i)];
        if (!s.resolved || latched[static_cast<size_t>(i)]) continue;
        plan.entries.push_back(PushEntry{ kPieces[i].piece, s.triple, s.n, copies });
    }
    return plan;
}

// A typed hit during an ordinary roll that pushed `plan`: n is the common
// count, so the coin is c / (n + c) whichever list was drawn.
inline bool AttributeHit(const KillPushPlan& plan, double t, double sub, double b, double u, Piece& out)
{
    return AttributeHitAmong(plan.entries, t, sub, b, u, out);
}

// ---- the kill draw: DropItem's own unique draw ---------------------------------------
//
// A kill's unique is not built by DropUniqueItems (Live 3: type 45's gate stays
// closed on ordinary kills) but by DropItem itself (the hub's RDM § 13.12; the
// research doc's Mod live 7 results and Phase 4 decision). DropItem draws from
// lootListUnique[k], k being what LoadSatanicDropTier answered in the same call
// (measured: `tier-k: matches`). It keeps an entry whose definition reads
// rarity 6 with fields 40 and 41 clear (static reading), and it keeps the first
// such entry it draws (measured, thinly: `draw-mode: first-kept`). An SS (tier
// 5) entry meets one more check; a pushed SS entry the draw picks is built, if
// rarely (measured: `ss-build: seen`).
//
// So, with the switch on, for the length of each outermost DropItem call each
// piece's draw stand-in goes onto every one of [0..5], `copies` times. A hit on
// a stand-in's whole triple is the piece's with probability c / (n_k + c): k the
// tier recorded in the innermost DropItem call in progress, n_k the stand-in's
// own count on list k. With no tier recorded the hit is refused and counted
// (` tier-unread=`). Each hit is settled before the next starts: a hit that is
// the piece's waits for its rewrite at CreateItemNew's entry, and the next hit
// drops it. A roll run inside the call (#74's Angelic roll, the shared
// DropUniqueItems detour, the Heroic hook) sees the lists as before: the adapter
// takes the call's entries off, and the core sets the call's hit state aside
// until that roll returns. The DropUniqueItems and Heroic routes stay as built.

// The draw reads one of [0..5].
inline constexpr int kDrawListCount = kListCount;
// The rarity the draw keeps, Satanic (static reading; `pool draw`'s rule).
inline constexpr int kDrawKeptRarity = 6;
// The SS tier (itemInfoStruct "32").
inline constexpr int kDrawSsTier = 5;
// The stand-in's tier preference (the owner, 2026-10-21, after Live 1b): SS
// first, so each piece is as likely as any one SS unique on its list, as the
// game's own SS set pieces are; below SS only for a class with no SS candidate.
// Reversing it is this one constant; no save holds it.
inline constexpr bool kDrawPrefersSs = true;
// No tier recorded in the call (or one that names no list).
inline constexpr int kDrawNoTier = -1;

// A triple's repository fields as the draw reads them, by name outside a roll:
// rarity (27), tier (32), and fields 40 and 41. False when they could not be
// read; such a triple is counted (` unread`) and never a stand-in.
struct DrawRead {
    int  rarity = 0;
    int  tier = kDrawNoTier;
    bool f40 = false;
    bool f41 = false;
};
using DrawReadFn = std::function<bool(const Triple& key, DrawRead& out)>;

// Whether the draw keeps an entry as read: rarity 6, fields 40 and 41 clear.
inline bool DrawKeeps(const DrawRead& r)
{
    return r.rarity == kDrawKeptRarity && !r.f40 && !r.f41;
}

using DrawCounts = std::array<int, kDrawListCount>;

struct DrawStandIn {
    bool       resolved = false;
    Triple     triple;
    int        rarity = 0;
    int        tier = kDrawNoTier;
    DrawCounts counts{};  // its entries on each of [0..5], repeats counted

    int Most() const
    {
        int most = 0;
        for (int n : counts) most = n > most ? n : most;
        return most;
    }
    bool Ss() const { return tier == kDrawSsTier; }
};

// Whether `a` is the better stand-in: the preferred tier side first, then the
// smaller largest count over the six lists (0 everywhere first), then the lower
// (sub, b).
inline bool DrawBetter(const DrawStandIn& a, const DrawStandIn& b)
{
    const bool pa = a.Ss() == kDrawPrefersSs;
    const bool pb = b.Ss() == kDrawPrefersSs;
    if (pa != pb) return pa;
    if (a.Most() != b.Most()) return a.Most() < b.Most();
    if (a.triple.sub != b.triple.sub) return a.triple.sub < b.triple.sub;
    return a.triple.b < b.triple.b;
}

struct DrawResolution {
    std::array<int, kDrawListCount>  lengths{};  // each list's entries, every kind
    int                              unread = 0; // distinct triples of a piece class whose fields could not be read
    std::array<DrawStandIn, kPieceCount> standIns{};
    std::array<int, kPieceCount>     candidates{};  // per class: the distinct triples the draw keeps
    std::array<int, kPieceCount>     ss{};          // of those, the SS ones

    bool AnyResolved() const
    {
        for (const DrawStandIn& s : standIns)
            if (s.resolved) return true;
        return false;
    }
};

// The draw's stand-in rule. For each piece's class: the distinct triples of
// that class on any of [0..5] the draw keeps as read, each read once; the
// choice is DrawBetter's. A class with no candidate has no draw stand-in: that
// piece does not drop from the draw, and the others still do. There is no
// equal-count rule: the coin uses the call's recorded k.
inline DrawResolution ResolveDrawStandIns(const KillLists& lists, const DrawReadFn& read)
{
    DrawResolution r;
    std::map<Triple, DrawCounts> counts;  // ordered by (type, sub, b)
    for (int k = 0; k < kDrawListCount; ++k) {
        const std::vector<ListEntry>& list = lists[static_cast<size_t>(k)];
        r.lengths[static_cast<size_t>(k)] = static_cast<int>(list.size());
        for (const ListEntry& e : list) {
            if (!e.triple) continue;
            Triple key;
            Piece p = Piece::Helmet;
            if (!Mark1Set::AsWhole(e.t, key.t) || !Mark1Set::AsWhole(e.sub, key.sub) || !Mark1Set::AsWhole(e.b, key.b)) continue;
            if (!PieceOfClass(key.t, p)) continue;
            ++counts[key][static_cast<size_t>(k)];  // zeros when new
        }
    }
    for (const auto& [key, c] : counts) {
        Piece p = Piece::Helmet;
        PieceOfClass(key.t, p);
        DrawRead v;
        if (!read || !read(key, v)) {
            ++r.unread;
            continue;
        }
        if (!DrawKeeps(v)) continue;
        const size_t i = static_cast<size_t>(p);
        ++r.candidates[i];
        DrawStandIn cand;
        cand.resolved = true;
        cand.triple = key;
        cand.rarity = v.rarity;
        cand.tier = v.tier;
        cand.counts = c;
        if (cand.Ss()) ++r.ss[i];
        DrawStandIn& s = r.standIns[i];
        if (!s.resolved || DrawBetter(cand, s)) s = cand;
    }
    return r;
}

// The draw's stand-ins, resolved again when any of [0..5] changed: another
// array (its id) or another length.
class DrawStandInCache {
public:
    bool Stale(const KillListKeys& keys) const
    {
        if (!have_) return true;
        for (size_t k = 0; k < keys.size(); ++k)
            if (keys[k].id != keys_[k].id || keys[k].length != keys_[k].length) return true;
        return false;
    }
    void Store(const KillListKeys& keys, const DrawResolution& r)
    {
        have_ = true;
        keys_ = keys;
        resolution_ = r;
    }
    const DrawResolution* Find() const { return have_ ? &resolution_ : nullptr; }

private:
    bool           have_ = false;
    KillListKeys   keys_{};
    DrawResolution resolution_;
};

struct DrawPushEntry {
    Piece      piece = Piece::Helmet;
    Triple     triple;
    DrawCounts counts{};  // the lists' own entries carrying the triple
    int        copies = 0;
};

// What one DropItem call pushes: `copies` entries per piece that has a draw
// stand-in and is not latched off, onto each of [0..5]. Copies below 1 push
// nothing; above kMaxCopies push kMaxCopies.
struct DrawPlan {
    std::vector<DrawPushEntry> entries;  // each one goes onto every list, [0] to [5]

    int PerListCount() const
    {
        int total = 0;
        for (const DrawPushEntry& e : entries) total += e.copies;
        return total;
    }
    int EntryCount() const { return PerListCount() * kDrawListCount; }
};

inline DrawPlan PlanDrawPush(const DrawResolution& r, int copies, const std::array<bool, kPieceCount>& latched)
{
    DrawPlan plan;
    if (copies < 1) return plan;
    if (copies > kMaxCopies) copies = kMaxCopies;
    for (int i = 0; i < kPieceCount; ++i) {
        const DrawStandIn& s = r.standIns[static_cast<size_t>(i)];
        if (!s.resolved || latched[static_cast<size_t>(i)]) continue;
        plan.entries.push_back(DrawPushEntry{ kPieces[i].piece, s.triple, s.counts, copies });
    }
    return plan;
}

// The share of a hit on list k that is the piece: c / (n_k + c); 0 when k names
// no list.
inline double DrawCoinShare(const DrawPushEntry& e, int k)
{
    if (k < 0 || k >= kDrawListCount) return 0.0;
    return CoinShare(e.counts[static_cast<size_t>(k)], e.copies);
}

// What a typed hit in a pushing DropItem call is.
enum class DrawHit : int {
    NotInScope,  // no call pushed, or its hit state is set aside: not the draw's to settle
    GameOwn,     // not a stand-in's triple, or lost on the coin
    Ours,        // the piece's
    TierUnread,  // a stand-in's triple with no tier recorded: refused, counted
};

// A typed hit among the entries a call pushed, with the call's recorded k.
inline DrawHit AttributeDrawHit(const DrawPlan& plan, int k, double t, double sub, double b, double u, Piece& out)
{
    Triple key;
    if (!Mark1Set::AsWhole(t, key.t) || !Mark1Set::AsWhole(sub, key.sub) || !Mark1Set::AsWhole(b, key.b)) return DrawHit::GameOwn;
    for (const DrawPushEntry& e : plan.entries) {
        if (!(e.triple == key)) continue;
        if (k < 0 || k >= kDrawListCount) return DrawHit::TierUnread;
        if (!(u >= 0.0 && u < 1.0) || !(u < DrawCoinShare(e, k))) return DrawHit::GameOwn;
        out = e.piece;
        return DrawHit::Ours;
    }
    return DrawHit::GameOwn;
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

// The ` gate=` and ` kill-gate=` fields, as the flight charms' is. Pending:
// the switch is on and installed, and that roll has not had its first
// resolving call yet. Each roll reads the lists it resolves from at its own
// call (the Heroic roll from its own sub-list argument), so there is nothing
// to resolve at switch-on (Live 1's `drops-on`).
enum class Gate : int { Off, On, Refused, NotResolved, NoDetection, Pending };

inline constexpr std::string_view GateName(Gate g)
{
    switch (g) {
    case Gate::Off:         return "off";
    case Gate::On:          return "on";
    case Gate::Refused:     return "refused";
    case Gate::NotResolved: return "not-resolved";
    case Gate::NoDetection: return "no-detection";
    case Gate::Pending:     return "pending";
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

// The kill draw's typed-build counters (the draw's positive control), over
// the CreateDefaultParams calls made in DropItem calls that carry the pieces'
// entries, each counted once before any coin: a typed pick (`standIn`, its
// whole triple a piece's own stand-in; `natural`, every other), a unique
// build no read matched (`untyped`), a build whose third argument does not
// read as true the way the game reads it (`normal`, never typed), and a build
// while the draw's scope was false,
// under its first false condition (busy, aside, lists, roll).
struct DrawTypedCounts {
    uint64_t standIn = 0;
    uint64_t natural = 0;
    uint64_t untyped = 0;
    uint64_t normal = 0;
    uint64_t busy = 0;
    uint64_t aside = 0;
    uint64_t lists = 0;
    uint64_t roll = 0;
    // The class fields (#166 replan 1), over the same in-scope builds as
    // typed, untyped and normal: the third argument's kind and truth as the
    // runner's truth rule reads it (`bool`, `num` - real, int32 and int64 -
    // each true / false; `none` absent, undefined or unset; `other` any other
    // kind), and the check of the gate's verdict against the `c` the game's
    // CreateDefaultParams stored (agree, disagree, unread).
    uint64_t boolTrue = 0;
    uint64_t boolFalse = 0;
    uint64_t numTrue = 0;
    uint64_t numFalse = 0;
    uint64_t none = 0;
    uint64_t other = 0;
    uint64_t checkAgree = 0;
    uint64_t checkDisagree = 0;
    uint64_t checkUnread = 0;
};

// Every field of the line. A default DropStatFields prints the fresh
// session's line; Drops::FillStat fills it but the four hook routes (`hook=`,
// `kill-hook=`, `draw-hook=`, `tier-hook=`) and the typed-build counters,
// which the adapter sets.
// `art-wait=`, `injected=`, `refused=` and `anomalies=` count every route; the
// rest are the Heroic roll's, the `kill-` ones the DropUniqueItems route's, and
// the `draw-` ones and `tier-unread=` the kill draw's.
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
    // The kill route.
    Gate        killGate = Gate::Off;
    std::string killHook = "not-installed";  // the shared DropUniqueItems detour's route
    uint64_t    killCalls = 0;               // every call the detour saw, switch on or off
    uint64_t    killPushedCalls = 0;
    uint64_t    killNested = 0;
    DropTally   killPieces[kPieceCount];
    std::array<StandIn, kPieceCount> killStandIns{};
    // The kill draw.
    Gate        drawGate = Gate::Off;
    std::string drawHook = "not-installed";  // DropManager's one DropItem detour's route
    std::string tierHook = "not-installed";  // raredrop's one LoadSatanicDropTier detour's route
    uint64_t    drawCalls = 0;               // every DropItem call the detour saw, switch on or off
    uint64_t    drawPushedCalls = 0;
    uint64_t    drawNested = 0;
    uint64_t    tierUnread = 0;              // stand-in hits refused with no tier recorded
    DropTally   drawPieces[kPieceCount];
    std::array<DrawStandIn, kPieceCount> drawStandIns{};
    DrawTypedCounts drawTyped;               // the adapter's typed-build counters
};

// A piece's stand-in as the line prints it: `<sub>:<b>@<rarity>x<n>`, or none.
inline std::string StandInText(const StandIn& s)
{
    if (!s.resolved) return "none";
    return std::to_string(s.triple.sub) + ":" + std::to_string(s.triple.b) + "@" + std::to_string(s.rarity)
        + "x" + std::to_string(s.n);
}

// A stand-in field: `pending` while its roll's gate is, else StandInText.
inline std::string StandInField(Gate g, const StandIn& s)
{
    return g == Gate::Pending ? std::string("pending") : StandInText(s);
}

// A draw stand-in as the line prints it, `<sub>:<b>@<rarity>/<tier>x<n0>,...,<n5>`
// (`pool draw`'s form, so the tier it was picked at shows), or none.
inline std::string DrawStandInText(const DrawStandIn& s)
{
    if (!s.resolved) return "none";
    std::string counts;
    for (size_t k = 0; k < s.counts.size(); ++k) counts += (k ? "," : "") + std::to_string(s.counts[k]);
    return std::to_string(s.triple.sub) + ":" + std::to_string(s.triple.b) + "@" + std::to_string(s.rarity) + "/"
        + std::to_string(s.tier) + "x" + counts;
}

inline std::string DrawStandInField(Gate g, const DrawStandIn& s)
{
    return g == Gate::Pending ? std::string("pending") : DrawStandInText(s);
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
        s += " standin-" + std::string(kPieces[i].key) + "=" + StandInField(f.gate, f.standIns[static_cast<size_t>(i)]);
    s += " list-refused=" + std::to_string(f.listRefused);
    s += " unread=" + std::to_string(f.unread);
    s += " kill-gate=" + std::string(GateName(f.killGate));
    s += " kill-hook=" + f.killHook;
    s += " kill-calls=" + std::to_string(f.killCalls);
    s += " kill-pushed=" + std::to_string(f.killPushedCalls);
    s += " kill-nested=" + std::to_string(f.killNested);
    for (int i = 0; i < kPieceCount; ++i)
        s += " kill-" + std::string(kPieces[i].key) + "=" + std::to_string(f.killPieces[i].ours) + "/"
            + std::to_string(f.killPieces[i].built);
    for (int i = 0; i < kPieceCount; ++i)
        s += " kill-standin-" + std::string(kPieces[i].key) + "="
            + StandInField(f.killGate, f.killStandIns[static_cast<size_t>(i)]);
    s += " draw-gate=" + std::string(GateName(f.drawGate));
    s += " draw-hook=" + f.drawHook;
    s += " tier-hook=" + f.tierHook;
    s += " draw-calls=" + std::to_string(f.drawCalls);
    s += " draw-pushed=" + std::to_string(f.drawPushedCalls);
    s += " draw-nested=" + std::to_string(f.drawNested);
    s += " tier-unread=" + std::to_string(f.tierUnread);
    for (int i = 0; i < kPieceCount; ++i)
        s += " draw-" + std::string(kPieces[i].key) + "=" + std::to_string(f.drawPieces[i].ours) + "/"
            + std::to_string(f.drawPieces[i].built);
    for (int i = 0; i < kPieceCount; ++i)
        s += " draw-standin-" + std::string(kPieces[i].key) + "="
            + DrawStandInField(f.drawGate, f.drawStandIns[static_cast<size_t>(i)]);
    const DrawTypedCounts& t = f.drawTyped;
    s += " draw-typed=" + std::to_string(t.standIn) + "/" + std::to_string(t.natural);
    s += " draw-untyped=" + std::to_string(t.untyped);
    s += " draw-normal=" + std::to_string(t.normal);
    s += " draw-outside=busy:" + std::to_string(t.busy) + ",aside:" + std::to_string(t.aside) + ",lists:"
        + std::to_string(t.lists) + ",roll:" + std::to_string(t.roll);
    s += " draw-class=bool:" + std::to_string(t.boolTrue) + "/" + std::to_string(t.boolFalse) + ",num:"
        + std::to_string(t.numTrue) + "/" + std::to_string(t.numFalse) + ",none:" + std::to_string(t.none) + ",other:"
        + std::to_string(t.other);
    s += " draw-class-check=" + std::to_string(t.checkAgree) + "/" + std::to_string(t.checkDisagree) + "/"
        + std::to_string(t.checkUnread);
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

    // The ` gate=` field (the Heroic roll). `detectionNative`: #74's detection
    // (InstallSignatureAngelicHooks) is all four inline. `pending` until the
    // roll's first resolving call.
    Gate GateNow(bool detectionNative) const { return GateOf(listState_, detectionNative); }
    // The ` kill-gate=` field (the ordinary roll), the same words; both rolls
    // attribute their hits through #74's detection.
    Gate KillGateNow(bool detectionNative) const { return GateOf(killState_, detectionNative); }

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
    // A removal that did not find the list as it pushed it (either roll).
    void NoteAnomaly() { ++anomalies_; }

    // ---- the kill route, per call ---------------------------------------------
    // Every call the shared DropUniqueItems detour sees is counted
    // (` kill-calls=`, switch on or off). `rollDepth` is g_SigRollDepth;
    // `uniqueDepth` the shared detour's own depth (a unique roll already in
    // progress). Either above 0 is nested: the game's own roll, counted. The
    // art-ready rule is the Heroic roll's.
    CallAction BeginKillCall(bool detectionNative, int rollDepth, int uniqueDepth, bool artReady)
    {
        ++killCalls_;
        if (!enabled_ || !installed_) return CallAction::PassOff;
        if (!detectionNative) return CallAction::PassGate;
        if (rollDepth > 0 || uniqueDepth > 0) {
            ++killNested_;
            return CallAction::PassNested;
        }
        if (!artReady) {
            ++artWait_;
            return CallAction::PassArtWait;
        }
        return CallAction::Resolve;
    }

    KillStandInCache&       KillCache() { return killCache_; }
    const KillStandInCache& KillCache() const { return killCache_; }

    // The lists could not be read: nothing pushed, the kill gate reads
    // not-resolved.
    void NoteKillListsUnreadable() { killState_ = ListState::Unreadable; }

    // The plan for a kill-route call whose lists resolved to `r`; it records
    // the resolution the line prints.
    KillPushPlan PlanKillFor(const KillResolution& r, int copies)
    {
        lastKillStandIns_ = r.standIns;
        killState_ = r.AnyResolved() ? ListState::Resolved : ListState::NoneResolved;
        return PlanKillPush(r, copies, latched_);
    }

    // The plan's entries went onto each of the five lists and were held
    // read-back.
    void NoteKillPushed(const KillPushPlan& p)
    {
        if (p.entries.empty()) return;
        ++killPushedCalls_;
        injected_ += static_cast<uint64_t>(p.EntryCount());
    }

    // ---- the kill draw, per DropItem call ---------------------------------------
    // Every call DropManager's DropItem detour sees is begun here and ended by
    // EndDrawCall when the original returns, whatever the answer (` draw-calls=`
    // counts each, switch on or off). The core keeps one tier slot per call in
    // progress, so LoadSatanicDropTier's answer goes to the innermost one.
    // `rollDepth` is g_SigRollDepth (#74's Angelic roll and the Heroic hook);
    // `uniqueDepth` the shared DropUniqueItems detour's. A call begun inside one
    // of those, or inside another DropItem call, passes (` draw-nested=`): a
    // DropItem call already in progress keeps the lists it pushed, and a hit in
    // the inner call is settled with the inner call's own k. The art-ready rule is
    // the Heroic roll's.
    CallAction BeginDrawCall(bool detectionNative, int rollDepth, int uniqueDepth, bool artReady)
    {
        ++drawCalls_;
        const bool inDraw = !drawKs_.empty();
        drawKs_.push_back(kDrawNoTier);
        if (!enabled_ || !installed_) return CallAction::PassOff;
        if (!detectionNative) return CallAction::PassGate;
        if (rollDepth > 0 || uniqueDepth > 0 || inDraw) {
            ++drawNested_;
            return CallAction::PassNested;
        }
        if (!artReady) {
            ++artWait_;
            return CallAction::PassArtWait;
        }
        return CallAction::Resolve;
    }

    DrawStandInCache&       DrawCache() { return drawCache_; }
    const DrawStandInCache& DrawCache() const { return drawCache_; }

    // The lists could not be read: nothing pushed, the draw gate reads
    // not-resolved.
    void NoteDrawListsUnreadable() { drawState_ = ListState::Unreadable; }

    // The plan for a call whose lists resolved to `r`; it records the
    // resolution the line prints.
    DrawPlan PlanDrawFor(const DrawResolution& r, int copies)
    {
        lastDrawStandIns_ = r.standIns;
        drawState_ = r.AnyResolved() ? ListState::Resolved : ListState::NoneResolved;
        return PlanDrawPush(r, copies, latched_);
    }

    // The plan's entries went onto each of [0..5] and were held read-back: the
    // call that began last is the one that pushed, and its hits are settled
    // against `p` until it ends.
    void NoteDrawPushed(const DrawPlan& p)
    {
        if (p.entries.empty() || drawKs_.empty() || frame_.active) return;
        ++drawPushedCalls_;
        injected_ += static_cast<uint64_t>(p.EntryCount());
        frame_ = DrawFrame{};
        frame_.active = true;
        frame_.depth = drawKs_.size();
        frame_.plan = p;
    }

    // A DropItem call in progress pushed: its plan, for the adapter's removal
    // (the tail rule) before EndDrawCall, and to put the entries back after a
    // nested roll.
    bool            DrawPushing() const { return frame_.active; }
    const DrawPlan* PushedDrawPlan() const { return frame_.active ? &frame_.plan : nullptr; }
    size_t          DrawDepth() const { return drawKs_.size(); }

    // LoadSatanicDropTier's answer (after raredrop's lift), for the innermost
    // DropItem call in progress: a whole number 0 to 5 is that call's k; any
    // other answer leaves it with none. Outside a DropItem call it is no one's.
    void NoteDrawTier(bool numeric, double value)
    {
        if (drawKs_.empty()) return;
        long long k = 0;
        const bool whole = numeric && Mark1Set::AsWhole(value, k) && k >= 0 && k < kDrawListCount;
        drawKs_.back() = whole ? static_cast<int>(k) : kDrawNoTier;
    }
    // The innermost call's k, or kDrawNoTier.
    int DrawTier() const { return drawKs_.empty() ? kDrawNoTier : drawKs_.back(); }

    // A roll begins inside a pushing call (#74's Angelic roll, the shared
    // DropUniqueItems detour, the Heroic hook). True when the call's entries
    // are to come off the lists now (the outermost such roll; counted in
    // ` draw-nested=`). Until RestoreDraw, the call's hit state is set aside: no
    // hit is the draw's, and a hit waiting for its rewrite keeps waiting.
    bool SetDrawAside()
    {
        if (!frame_.active) return false;
        if (++frame_.aside != 1) return false;
        ++drawNested_;
        return true;
    }
    // That roll returned. True when the entries are to go back on the lists
    // now (the outermost such roll returned).
    bool RestoreDraw()
    {
        if (!frame_.active || frame_.aside == 0) return false;
        return --frame_.aside == 0;
    }
    bool DrawAside() const { return frame_.active && frame_.aside > 0; }

    // A typed hit (a CreateDefaultParams call typed by the scope's latest
    // repository read) in scope `draw`. It first settles the call's previous
    // hit: a piece's hit that was never rewritten is dropped. A stand-in's hit
    // with no tier recorded is refused (` tier-unread=`); one that falls the
    // piece's is counted `ours` on the draw's tally and waits for
    // TakeDrawRewrite.
    DrawHit SettleDrawHit(double t, double sub, double b, double u, Piece& out)
    {
        if (!frame_.active || frame_.aside > 0) return DrawHit::NotInScope;
        frame_.pending = false;
        const DrawHit h = AttributeDrawHit(frame_.plan, DrawTier(), t, sub, b, u, out);
        if (h == DrawHit::TierUnread) ++tierUnread_;
        if (h == DrawHit::Ours) {
            frame_.pending = true;
            frame_.pendingPiece = out;
            ++drawTally_[static_cast<size_t>(out)].ours;
        }
        return h;
    }
    // At CreateItemNew's entry: the piece the current hit is to be rewritten
    // to, once; false when no hit of the draw's is waiting (or it is set aside).
    bool TakeDrawRewrite(Piece& out)
    {
        if (!frame_.active || frame_.aside > 0 || !frame_.pending) return false;
        out = frame_.pendingPiece;
        frame_.pending = false;
        return true;
    }

    // The call that began last returns. True when it was the call that
    // pushed: its hit state ends here, and the adapter has taken its entries
    // off before calling this.
    bool EndDrawCall()
    {
        if (drawKs_.empty()) return false;
        const size_t depth = drawKs_.size();
        drawKs_.pop_back();
        if (!frame_.active || depth != frame_.depth) return false;
        frame_ = DrawFrame{};
        return true;
    }

    // A stand-in hit that became the piece, and the Custom Forge's final pass
    // counting it built, on the roll it came from. The draw's `ours` is
    // counted by SettleDrawHit.
    void NoteOurs(Piece p, Roll roll = Roll::Heroic) { ++TallyOf(roll)[static_cast<size_t>(p)].ours; }
    void NoteBuilt(Piece p, Roll roll = Roll::Heroic) { ++TallyOf(roll)[static_cast<size_t>(p)].built; }
    // A refused rewrite latches that piece off for the session on both rolls,
    // as #74 does: the rewrite is the same on either.
    void NoteRewriteRefused(Piece p)
    {
        ++rewriteRefused_;
        latched_[static_cast<size_t>(p)] = true;
    }
    bool Latched(Piece p) const { return latched_[static_cast<size_t>(p)]; }

    uint64_t HeroicCalls() const { return heroicCalls_; }
    uint64_t PushedCalls() const { return pushedCalls_; }
    uint64_t KillCalls() const { return killCalls_; }
    uint64_t KillPushedCalls() const { return killPushedCalls_; }
    uint64_t DrawCalls() const { return drawCalls_; }
    uint64_t DrawPushedCalls() const { return drawPushedCalls_; }
    uint64_t DrawNested() const { return drawNested_; }
    uint64_t TierUnread() const { return tierUnread_; }

    // The ` draw-gate=` field (the kill draw), the same words: `pending` until
    // the draw's first resolving call.
    Gate DrawGateNow(bool detectionNative) const { return GateOf(drawState_, detectionNative); }

    // Every field but `hook=`, `kill-hook=`, `draw-hook=` and `tier-hook=`, which
    // the adapter sets.
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
        f.killGate = KillGateNow(detectionNative);
        f.killCalls = killCalls_;
        f.killPushedCalls = killPushedCalls_;
        f.killNested = killNested_;
        for (int i = 0; i < kPieceCount; ++i) f.killPieces[i] = killTally_[static_cast<size_t>(i)];
        f.killStandIns = lastKillStandIns_;
        f.drawGate = DrawGateNow(detectionNative);
        f.drawCalls = drawCalls_;
        f.drawPushedCalls = drawPushedCalls_;
        f.drawNested = drawNested_;
        f.tierUnread = tierUnread_;
        for (int i = 0; i < kPieceCount; ++i) f.drawPieces[i] = drawTally_[static_cast<size_t>(i)];
        f.drawStandIns = lastDrawStandIns_;
    }

private:
    // Unknown: the roll has had no resolving call yet (the gate reads pending).
    enum class ListState : int { Unknown, Resolved, NoneResolved, Unreadable };

    // The DropItem call that pushed: its plan, the depth it began at, how many
    // rolls inside it have set it aside, and the hit waiting for its rewrite.
    struct DrawFrame {
        bool     active = false;
        size_t   depth = 0;
        DrawPlan plan;
        int      aside = 0;
        bool     pending = false;
        Piece    pendingPiece = Piece::Helmet;
    };

    Gate GateOf(ListState state, bool detectionNative) const
    {
        if (refused_) return Gate::Refused;
        if (!enabled_ || !installed_) return Gate::Off;
        if (!detectionNative) return Gate::NoDetection;
        if (state == ListState::Unknown) return Gate::Pending;
        if (state == ListState::NoneResolved || state == ListState::Unreadable) return Gate::NotResolved;
        return Gate::On;
    }
    std::array<DropTally, kPieceCount>& TallyOf(Roll roll)
    {
        if (roll == Roll::Kill) return killTally_;
        if (roll == Roll::Draw) return drawTally_;
        return tally_;
    }

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
    // The kill route.
    ListState    killState_ = ListState::Unknown;
    std::array<StandIn, kPieceCount>   lastKillStandIns_{};
    std::array<DropTally, kPieceCount> killTally_{};
    KillStandInCache killCache_;
    uint64_t     killCalls_ = 0;
    uint64_t     killPushedCalls_ = 0;
    uint64_t     killNested_ = 0;
    // The kill draw.
    ListState    drawState_ = ListState::Unknown;
    std::array<DrawStandIn, kPieceCount> lastDrawStandIns_{};
    std::array<DropTally, kPieceCount>   drawTally_{};
    DrawStandInCache drawCache_;
    std::vector<int> drawKs_;   // one tier slot per DropItem call in progress, innermost last
    DrawFrame    frame_;
    uint64_t     drawCalls_ = 0;
    uint64_t     drawPushedCalls_ = 0;
    uint64_t     drawNested_ = 0;
    uint64_t     tierUnread_ = 0;
};

// `mark1set drops 1|0`: the drops switch, and on a switch-on the pieces' art,
// armed for the session as `mark1set bonus 1` arms it, so a dropped piece shows
// the owner's art. One flag, armed by either switch (Bonus::ArtArmed).
inline void SetSwitch(Drops& drops, Mark1Set::Bonus& set, bool on)
{
    drops.SetEnabled(on);
    set.NoteDropsSwitch(on);
}

// The reply to `mark1set drops 1|0`, after the adapter has tried its install:
// the switch, the install, and each route's gate (the Heroic roll, the
// DropUniqueItems route, the kill draw), `pending` until that route's first
// resolving call.
inline std::string DropsReply(const Drops& d, bool detectionNative)
{
    return "mark1set: drops=" + std::string(d.Enabled() ? "on" : "off") + " install=" + std::string(d.InstallStateName())
        + " gate=" + std::string(GateName(d.GateNow(detectionNative)))
        + " kill-gate=" + std::string(GateName(d.KillGateNow(detectionNative)))
        + " draw-gate=" + std::string(GateName(d.DrawGateNow(detectionNative)));
}

} // namespace ForgePact::Mark1Drops
