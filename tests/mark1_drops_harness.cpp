// Behavioral regression harness for the Mark 1 drops' core
// (Mark1Drops.hpp, ForgePact #166 phase 3).
//
// The Python runner injects the REAL ForgePact::Mark1Drops header below; it
// includes Mark1Set.hpp, the pieces' table, from plugin/include. No game is
// touched: a sub-list is a vector of entries the scenarios fill, and the
// repository's verdict on each triple a callback that counts its calls.
//
// Baseline (labels baseline/*): the switch off pushes nothing and attributes
// nothing, the hook staying in passes through, and a hit on a would-be
// stand-in with nothing pushed stays the game's own. Targets: the sub-list a
// call reads (list_index/*); the stand-in rule, Heroic preferred, a class with
// none, ties by the lowest (sub, b), n counted with repeats, and the cache
// (standin/*); the coin c / (n + c), the attribution and the rewrite target
// per piece from Mark1Set.hpp's kPieces (coin/*); the gate and install words
// (gate/*); the art-ready test (art_ready/*); the `mark1set drops:` line,
// fresh and filled, and the reply (stat/*). The kill route on the ordinary
// unique roll (kill/*): its switch-off baseline, the ordinary roll's filter,
// the equal-count stand-in rule over the six lists (count 0 preferred, then
// the smaller count, ties by the lowest sub and b, a class with none), the
// push onto each of [0..4], the coin with the common count, the cache over
// all six lists, nested calls, the art-ready rule and the line's kill fields.
// Each roll's gate and stand-ins read `pending` until that roll's first
// resolving call (pending/*, Live 1's `drops-on`). The kill draw, DropItem's own
// unique draw (draw/*): the switch-off baseline, a call that pushed nothing and a
// call begun inside a roll; the draw's filter and the SS-first stand-in rule over
// the six lists, the push onto each of [0..5], the coin c / (n_k + c) from the
// call's recorded k, no tier refused, two hits settled apart, a nested roll
// setting the call aside, the tier going to the innermost call, and the line.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// PRODUCTION_MARK1DROPS

using namespace ForgePact::Mark1Drops;
namespace M1s = ForgePact::Mark1Set;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::vector<std::string> words(const std::string& s)
{
    std::vector<std::string> out;
    std::string w;
    for (char c : s) {
        if (c == ' ') { if (!w.empty()) out.push_back(w); w.clear(); }
        else w += c;
    }
    if (!w.empty()) out.push_back(w);
    return out;
}

static bool startsWith(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }

static std::vector<std::string> fieldNames(const std::string& line)
{
    std::vector<std::string> out;
    for (const std::string& w : words(line)) {
        const size_t eq = w.find('=');
        if (eq != std::string::npos) out.push_back(w.substr(0, eq));
    }
    return out;
}

static std::string field(const std::string& line, const std::string& name)
{
    for (const std::string& w : words(line))
        if (w.rfind(name + "=", 0) == 0) return w.substr(name.size() + 1);
    return "<missing>";
}

static ListEntry entry(double t, double sub, double b)
{
    ListEntry e;
    e.triple = true;
    e.t = t;
    e.sub = sub;
    e.b = b;
    return e;
}

static ListEntry notATriple()
{
    return ListEntry{};
}

// The repository, as a table of (type, sub, b) -> rarity, hidden; a triple
// missing from it cannot be read. Counts every question asked.
struct FakeRepo {
    struct Row {
        Triple key;
        int    rarity;
        bool   hidden;
    };
    std::vector<Row> rows;
    int calls = 0;
    std::vector<Triple> asked;
    void add(long long t, long long sub, long long b, int rarity, bool hidden = false)
    {
        Triple k;
        k.t = t;
        k.sub = sub;
        k.b = b;
        rows.push_back(Row{ k, rarity, hidden });
    }
    VerdictFn fn()
    {
        return [this](const Triple& key, int& rarity, bool& hidden) {
            ++calls;
            asked.push_back(key);
            for (const Row& r : rows) {
                if (!(r.key == key)) continue;
                rarity = r.rarity;
                hidden = r.hidden;
                return true;
            }
            return false;
        };
    }
};

static bool isTriple(const StandIn& s, long long t, long long sub, long long b)
{
    return s.resolved && s.triple.t == t && s.triple.sub == sub && s.triple.b == b;
}

// A list with one Heroic stand-in per piece class, each listed once, among
// other classes' and other rarities' entries.
static std::vector<ListEntry> fullList(FakeRepo& repo)
{
    repo.add(0, 3, 17, 9);   // helmet
    repo.add(1, 2, 40, 9);   // body armor
    repo.add(2, 1, 8, 9);    // boots
    repo.add(4, 0, 5, 9);    // gloves
    repo.add(10, 0, 47, 7);  // a charm the roll refuses (Angelic)
    return { entry(0, 3, 17), entry(10, 0, 47), entry(1, 2, 40), entry(2, 1, 8), notATriple(), entry(4, 0, 5) };
}

// The six sub-lists as the kill route reads them, among a charm on every list
// (never asked about) and an entry that is not a triple:
//   helmet: (0,3,17) Satanic on [5] only, n 0; (0,1,2) Satanic once on each of [0..4], n 1;
//   body:   (1,2,40) Satanic once on each of [0..4], n 1; (1,0,1) twice on [0], once elsewhere: unequal;
//   boots:  (2,1,8) Heroic on [5], refused by the ordinary roll; (2,4,4) and (2,4,6) rarity 5, twice on
//           each of [0..4], n 2: a tie, the lowest (sub, b) wins;
//   gloves: (4,0,5) Heroic and (4,0,6) hidden, both on [5]: none.
static KillLists killLists(FakeRepo& repo)
{
    repo.add(0, 3, 17, 6);
    repo.add(0, 1, 2, 6);
    repo.add(1, 2, 40, 6);
    repo.add(1, 0, 1, 6);
    repo.add(2, 1, 8, 9);
    repo.add(2, 4, 4, 5);
    repo.add(2, 4, 6, 5);
    repo.add(4, 0, 5, 9);
    repo.add(4, 0, 6, 6, true);
    repo.add(10, 0, 47, 6);
    KillLists lists;
    for (int k = 0; k < kKillListCount; ++k) {
        std::vector<ListEntry>& l = lists[static_cast<size_t>(k)];
        l = { entry(10, 0, 47), entry(0, 1, 2), entry(1, 2, 40), entry(1, 0, 1), entry(2, 4, 4), entry(2, 4, 4),
              entry(2, 4, 6), entry(2, 4, 6), notATriple() };
        if (k == 0) l.push_back(entry(1, 0, 1));
    }
    lists[5] = { entry(0, 3, 17), entry(2, 1, 8), entry(4, 0, 5), entry(4, 0, 6), entry(10, 0, 47) };
    return lists;
}

static KillListKeys keysOf(const KillLists& lists)
{
    KillListKeys keys{};
    for (size_t k = 0; k < keys.size(); ++k) {
        keys[k].id = 100 + k;
        keys[k].length = static_cast<int>(lists[k].size());
    }
    return keys;
}

// The repository as the kill draw reads it: (type, sub, b) -> rarity, tier,
// fields 40 and 41; a triple missing from it cannot be read. Counts every
// question asked.
struct FakeDrawRepo {
    struct Row {
        Triple   key;
        DrawRead read;
    };
    std::vector<Row> rows;
    int calls = 0;
    std::vector<Triple> asked;
    void add(long long t, long long sub, long long b, int rarity, int tier, bool f40 = false, bool f41 = false)
    {
        Triple k;
        k.t = t;
        k.sub = sub;
        k.b = b;
        DrawRead r;
        r.rarity = rarity;
        r.tier = tier;
        r.f40 = f40;
        r.f41 = f41;
        rows.push_back(Row{ k, r });
    }
    DrawReadFn fn()
    {
        return [this](const Triple& key, DrawRead& out) {
            ++calls;
            asked.push_back(key);
            for (const Row& r : rows) {
                if (!(r.key == key)) continue;
                out = r.read;
                return true;
            }
            return false;
        };
    }
};

static bool isDraw(const DrawStandIn& s, long long t, long long sub, long long b)
{
    return s.resolved && s.triple.t == t && s.triple.sub == sub && s.triple.b == b;
}

static DrawCounts countsOf(int n0, int n1, int n2, int n3, int n4, int n5)
{
    return DrawCounts{ n0, n1, n2, n3, n4, n5 };
}

// The six lists as the kill draw reads them, among a charm on every list (never
// asked about) and an entry that is not a triple:
//   helmet: (0,3,17) SS, twice on each list; (0,1,2) tier 3, once on [5]: SS wins
//           despite the larger count; (0,0,1) SS with field 40 set: never;
//   body:   no SS candidate. (1,2,40) tier 4 at 1,0,2,0,0,0; (1,5,5) tier 2 once on
//           each list: the smaller largest count wins below SS; (1,0,0) Heroic: never;
//   boots:  (2,4,4) and (2,4,6) SS, once on [3] each: a tie, the lowest (sub, b);
//           (2,1,8) SS with field 41 set: never;
//   gloves: (4,0,5) Heroic, and (4,0,6) unreadable: none.
static KillLists drawLists(FakeDrawRepo& repo)
{
    repo.add(0, 3, 17, 6, 5);
    repo.add(0, 1, 2, 6, 3);
    repo.add(0, 0, 1, 6, 5, true, false);
    repo.add(1, 2, 40, 6, 4);
    repo.add(1, 5, 5, 6, 2);
    repo.add(1, 0, 0, 9, 5);
    repo.add(2, 4, 4, 6, 5);
    repo.add(2, 4, 6, 6, 5);
    repo.add(2, 1, 8, 6, 5, false, true);
    repo.add(4, 0, 5, 9, 5);
    repo.add(10, 0, 47, 6, 5);
    KillLists lists;
    for (int k = 0; k < kDrawListCount; ++k)
        lists[static_cast<size_t>(k)] = { entry(10, 0, 47), entry(0, 3, 17), entry(0, 3, 17), notATriple() };
    auto add = [&lists](int k, std::initializer_list<ListEntry> more) {
        std::vector<ListEntry>& l = lists[static_cast<size_t>(k)];
        l.insert(l.end(), more.begin(), more.end());
    };
    add(0, { entry(1, 2, 40), entry(1, 5, 5), entry(1, 0, 0) });
    add(1, { entry(1, 5, 5) });
    add(2, { entry(1, 2, 40), entry(1, 2, 40), entry(1, 5, 5) });
    add(3, { entry(1, 5, 5), entry(2, 4, 4), entry(2, 4, 6), entry(2, 1, 8) });
    add(4, { entry(1, 5, 5), entry(4, 0, 5), entry(4, 0, 6) });
    add(5, { entry(1, 5, 5), entry(0, 1, 2), entry(0, 0, 1) });
    return lists;
}

static Drops installedOn()
{
    Drops d;
    d.SetEnabled(true);
    d.NoteInstall(true, false);
    return d;
}

static const std::array<bool, kPieceCount> kNoLatch{};

// An installed core inside one DropItem call that pushed drawLists' plan.
static Drops pushingCall(int copies = kPlayerCopies)
{
    Drops d = installedOn();
    FakeDrawRepo repo;
    const DrawResolution r = ResolveDrawStandIns(drawLists(repo), repo.fn());
    d.BeginDrawCall(true, 0, 0, true);
    d.NoteDrawPushed(d.PlanDrawFor(r, copies));
    return d;
}

int main()
{
    // ---- baseline: the switch off is the game's own roll ------------------------------
    {
        Drops d;
        int k = 99;
        const CallAction a = d.BeginCall(true, 0, true, ArgKind::Undefined, 0, k);
        DropStatFields f;
        d.FillStat(f, true);
        check("baseline/switch_off_passes_and_only_counts_the_call",
              a == CallAction::PassOff && k == kListRefused && f.heroicCalls == 1 && f.pushedCalls == 0
              && f.injected == 0 && f.nested == 0 && f.artWait == 0 && f.listRefused == 0, DropStatLine(f));

        // The hook stays in after the switch goes off, and passes through.
        Drops off = installedOn();
        off.SetEnabled(false);
        const CallAction b = off.BeginCall(true, 0, true, ArgKind::Undefined, 0, k);
        check("baseline/installed_switch_off_passes_through", b == CallAction::PassOff && off.Installed()
              && off.HeroicCalls() == 1 && off.PushedCalls() == 0);

        // Nothing pushed attributes nothing, whatever the triple and the coin.
        PushPlan empty;
        Piece p = Piece::Helmet;
        bool none = true;
        for (double u : { 0.0, 0.25, 0.5, 0.999 })
            if (AttributeHit(empty, 0, 3, 17, u, p)) none = false;
        check("baseline/nothing_pushed_attributes_nothing", none && empty.EntryCount() == 0);

        // A hit on what would be a stand-in, with nothing pushed for this call
        // (the switch off), stays the game's own.
        FakeRepo repo;
        const auto list = fullList(repo);
        const Resolution r = ResolveStandIns(5, list, repo.fn());
        const PushPlan zero = PlanPush(r, 0, kNoLatch);
        check("baseline/a_stand_in_hit_with_nothing_pushed_stays_the_games",
              zero.entries.empty() && !AttributeHit(zero, 0, 3, 17, 0.0, p) && !AttributeHit(PushPlan{}, 4, 0, 5, 0.0, p));
    }

    // ---- list_index: the sub-list a call reads ----------------------------------------
    {
        bool ok = kHeroicDefaultListIndex == 5 && kListCount == 6
            && ListIndexOf(ArgKind::Absent, 0) == 5 && ListIndexOf(ArgKind::Undefined, 3) == 5;
        check("list_index/absent_or_undefined_reads_5", ok);
        ok = true;
        for (int i = 0; i < 6; ++i)
            if (ListIndexOf(ArgKind::Number, i) != i) ok = false;
        check("list_index/whole_0_to_5_read_themselves", ok);
        const double inf = std::numeric_limits<double>::infinity();
        ok = ListIndexOf(ArgKind::Number, 6) == kListRefused && ListIndexOf(ArgKind::Number, -1) == kListRefused
            && ListIndexOf(ArgKind::Number, 2.5) == kListRefused && ListIndexOf(ArgKind::Number, std::nan("")) == kListRefused
            && ListIndexOf(ArgKind::Number, inf) == kListRefused && ListIndexOf(ArgKind::Number, 1e300) == kListRefused
            && ListIndexOf(ArgKind::Other, 5) == kListRefused && ListIndexOf(ArgKind::Other, 0) == kListRefused;
        check("list_index/anything_else_refused", ok);

        Drops d = installedOn();
        int k = 0;
        const CallAction a = d.BeginCall(true, 0, true, ArgKind::Number, 7, k);
        const CallAction b = d.BeginCall(true, 0, true, ArgKind::Number, 2, k);
        const int two = k;
        DropStatFields f;
        d.FillStat(f, true);
        check("list_index/a_refused_index_pushes_nothing_and_is_counted",
              a == CallAction::PassListRefused && b == CallAction::Resolve && two == 2 && f.listRefused == 1
              && f.heroicCalls == 2 && f.pushedCalls == 0, DropStatLine(f));
    }

    // ---- standin: the rule -------------------------------------------------------------
    {
        // One Heroic triple per class: each piece's stand-in is its own class's.
        FakeRepo repo;
        const auto list = fullList(repo);
        const Resolution r = ResolveStandIns(5, list, repo.fn());
        bool ok = r.index == 5 && r.length == 6 && r.unread == 0
            && isTriple(r.standIns[0], 0, 3, 17) && isTriple(r.standIns[1], 1, 2, 40)
            && isTriple(r.standIns[2], 2, 1, 8) && isTriple(r.standIns[3], 4, 0, 5);
        for (const StandIn& s : r.standIns)
            if (s.n != 1 || s.rarity != 9) ok = false;
        check("standin/one_per_piece_of_its_own_class", ok);
        // Only piece classes are asked about, each distinct triple once.
        ok = repo.calls == 4;
        for (const Triple& t : repo.asked)
            if (t.t == 10) ok = false;
        check("standin/only_piece_classes_asked_each_once", ok, std::to_string(repo.calls));

        // Heroic preferred over any other kept rarity, even at a higher (sub, b).
        FakeRepo h;
        h.add(0, 0, 1, 6);
        h.add(0, 3, 4, 9);
        h.add(0, 0, 0, 2);
        const Resolution rh = ResolveStandIns(5, { entry(0, 0, 1), entry(0, 3, 4), entry(0, 0, 0) }, h.fn());
        check("standin/heroic_preferred", isTriple(rh.standIns[0], 0, 3, 4) && rh.standIns[0].rarity == 9
              && rh.kept[0].keptTriples == 3);
        // With no Heroic, the lowest (sub, b) of any kept rarity.
        FakeRepo s;
        s.add(1, 2, 0, 6);
        s.add(1, 1, 9, 5);
        const Resolution rs = ResolveStandIns(5, { entry(1, 2, 0), entry(1, 1, 9) }, s.fn());
        check("standin/no_heroic_takes_the_lowest_kept", isTriple(rs.standIns[1], 1, 1, 9) && rs.standIns[1].rarity == 5);

        // Ties among Heroic triples: the lowest sub, then the lowest b.
        FakeRepo t;
        t.add(2, 2, 5, 9);
        t.add(2, 1, 9, 9);
        t.add(2, 1, 3, 9);
        const Resolution rt = ResolveStandIns(5, { entry(2, 2, 5), entry(2, 1, 9), entry(2, 1, 3) }, t.fn());
        check("standin/ties_by_lowest_sub_then_b", isTriple(rt.standIns[2], 2, 1, 3));

        // n counts the list's entries carrying the triple, repeats included.
        FakeRepo n;
        n.add(4, 0, 5, 9);
        const Resolution rn = ResolveStandIns(5, { entry(4, 0, 5), entry(4, 0, 5), entry(10, 0, 1), entry(4, 0, 5) }, n.fn());
        check("standin/n_counted_with_repeats", isTriple(rn.standIns[3], 4, 0, 5) && rn.standIns[3].n == 3
              && rn.kept[3].keptEntries == 3 && rn.kept[3].keptTriples == 1 && n.calls == 1);

        // The roll refuses Angelic, Unholy and hidden entries: none stands in.
        FakeRepo x;
        x.add(0, 0, 1, 7);
        x.add(0, 0, 2, 10);
        x.add(0, 0, 3, 9, true);
        const Resolution rx = ResolveStandIns(5, { entry(0, 0, 1), entry(0, 0, 2), entry(0, 0, 3) }, x.fn());
        check("standin/refused_rarities_and_hidden_never_stand_in", !rx.standIns[0].resolved && rx.kept[0].keptTriples == 0
              && HeroicKeeps(9, false) && HeroicKeeps(6, false) && !HeroicKeeps(7, false) && !HeroicKeeps(10, false)
              && !HeroicKeeps(9, true) && kRefusedRarities[0] == 7 && kRefusedRarities[1] == 10);

        // A class with none: that piece has no stand-in, the others still push.
        FakeRepo c;
        c.add(0, 3, 17, 9);
        c.add(1, 2, 40, 9);
        c.add(2, 1, 8, 9);
        const Resolution rc = ResolveStandIns(5, { entry(0, 3, 17), entry(1, 2, 40), entry(2, 1, 8) }, c.fn());
        const PushPlan pc = PlanPush(rc, kPlayerCopies, kNoLatch);
        ok = !rc.standIns[3].resolved && rc.AnyResolved() && pc.entries.size() == 3 && pc.EntryCount() == 3;
        for (const PushEntry& e : pc.entries)
            if (e.piece == Piece::Gloves) ok = false;
        check("standin/a_class_with_none_leaves_the_others", ok);

        // An unreadable verdict is counted and never a stand-in; entries that
        // are not whole-number triples are skipped but counted in the length.
        FakeRepo u;
        u.add(0, 5, 5, 9);
        const Resolution ru = ResolveStandIns(3, { entry(0, 1, 1), entry(0, 5, 5), notATriple(), entry(0, 0.5, 1) }, u.fn());
        check("standin/unreadable_counted_and_never_a_stand_in", ru.unread == 1 && isTriple(ru.standIns[0], 0, 5, 5)
              && ru.length == 4 && ru.index == 3 && u.calls == 2);
        const Resolution rnull = ResolveStandIns(5, { entry(0, 1, 1) }, VerdictFn{});
        check("standin/no_verdict_reader_is_unread", rnull.unread == 1 && !rnull.AnyResolved());

        // The plan: copies per resolved piece, latched pieces left out.
        std::array<bool, kPieceCount> latch{};
        latch[1] = true;
        const PushPlan p3 = PlanPush(r, 3, latch);
        ok = p3.index == 5 && p3.entries.size() == 3 && p3.EntryCount() == 9;
        for (const PushEntry& e : p3.entries)
            if (e.piece == Piece::Body || e.copies != 3 || e.n != 1) ok = false;
        check("standin/plan_copies_per_piece_latched_left_out", ok);
        check("standin/plan_copies_bounded", PlanPush(r, 0, kNoLatch).entries.empty()
              && PlanPush(r, -2, kNoLatch).entries.empty() && PlanPush(r, 1000, kNoLatch).entries[0].copies == kMaxCopies
              && PlanPush(r, kPlayerCopies, kNoLatch).EntryCount() == 4 && kPlayerCopies == 1 && kMaxCopies == 400);

        // The cache: stale when empty, when the list's id or length changed;
        // lists are kept apart.
        StandInCache cache;
        ok = cache.Stale(5, 11, 6) && cache.Find(5) == nullptr;
        cache.Store(11, r);
        ok = ok && !cache.Stale(5, 11, 6) && cache.Stale(5, 12, 6) && cache.Stale(5, 11, 7) && cache.Stale(4, 11, 6)
            && cache.Find(5) != nullptr && cache.Find(5)->standIns[0].triple.b == 17 && cache.Find(4) == nullptr
            && cache.Stale(-1, 11, 6) && cache.Stale(6, 11, 6) && cache.Find(6) == nullptr;
        check("standin/cache_resolves_again_when_id_or_length_changes", ok);
    }

    // ---- coin: c / (n + c), the hit and the rewrite ----------------------------------------
    {
        bool ok = CoinShare(1, 1) == 0.5 && CoinShare(0, 1) == 1.0 && CoinShare(3, 1) == 0.25
            && CoinShare(2, 2) == 0.5 && CoinShare(-1, 1) == 0.0 && CoinShare(1, 0) == 0.0;
        check("coin/share_is_c_over_n_plus_c", ok);
        ok = CoinOurs(1, 1, 0.0) && CoinOurs(1, 1, 0.4999) && !CoinOurs(1, 1, 0.5) && !CoinOurs(1, 1, 0.99)
            && !CoinOurs(1, 1, -0.1) && !CoinOurs(1, 1, 1.0) && !CoinOurs(1, 1, std::nan("")) && CoinOurs(0, 1, 0.999);
        check("coin/ours_below_the_share", ok);

        FakeRepo repo;
        const auto list = fullList(repo);
        const PushPlan plan = PlanPush(ResolveStandIns(5, list, repo.fn()), kPlayerCopies, kNoLatch);
        Piece p = Piece::Helmet;
        ok = AttributeHit(plan, 4, 0, 5, 0.2, p) && p == Piece::Gloves
            && AttributeHit(plan, 0, 3, 17, 0.0, p) && p == Piece::Helmet
            && !AttributeHit(plan, 0, 3, 17, 0.5, p)   // lost on the coin: the stand-in's own
            && !AttributeHit(plan, 1, 3, 17, 0.0, p)   // the type differs
            && !AttributeHit(plan, 0, 4, 17, 0.0, p)   // the sub differs
            && !AttributeHit(plan, 0, 3, 18, 0.0, p)   // the b differs
            && !AttributeHit(plan, 0, 3, 17.5, 0.0, p);
        check("coin/a_hit_on_the_whole_triple_is_the_piece_on_the_coin", ok);

        // The rewrite target per piece, from Mark1Set.hpp's kPieces.
        ok = true;
        for (const PieceRow& row : M1s::kPieces) {
            const RewriteTarget t = RewriteFor(row.piece);
            std::cout << "REWRITE " << row.key << " type=" << t.itemType << " a=" << t.a << " b=" << t.b
                      << " c=" << t.c << " j=" << t.j << std::endl;
            Piece back = Piece::Helmet;
            if (t.itemType != row.itemType || t.a != row.seed || t.b != row.baseB || t.c != 0 || t.j != 0) ok = false;
            if (!M1s::IsPiece(t.itemType, static_cast<double>(t.a), t.b, t.c, t.j, back) || back != row.piece) ok = false;
        }
        check("coin/rewrite_target_is_the_pieces_own_definition", ok);
    }

    // ---- gate: the install and the gate words ---------------------------------------------
    {
        Drops d;
        bool ok = d.InstallStateName() == "not-armed" && d.GateNow(true) == Gate::Off && !d.TickNeeded()
            && !d.ShouldInstall(true, true, false);
        check("gate/fresh_is_not_armed_and_off", ok);

        d.SetEnabled(true);
        ok = d.InstallStateName() == "waiting-for-character" && d.GateNow(true) == Gate::Off && d.TickNeeded()
            && !d.ShouldInstall(true, false, true) && !d.ShouldInstall(false, true, true) && d.ShouldInstall(true, true, true)
            && Drops::LooksForPlayer(0) && !Drops::LooksForPlayer(1) && Drops::LooksForPlayer(60);
        check("gate/switch_on_without_a_character_waits", ok);

        Drops r;
        r.SetEnabled(true);
        r.NoteInstall(false, true);
        ok = r.InstallStateName() == "refused" && !r.Enabled() && r.GateNow(true) == Gate::Refused && !r.TickNeeded();
        r.SetEnabled(true);
        ok = ok && !r.ShouldInstall(true, true, true) && r.ShouldInstall(true, true, false);
        r.NoteInstall(true, false);
        // Installed, and no roll resolved yet: pending (Live 1's `drops-on`).
        ok = ok && r.InstallStateName() == "installed" && r.GateNow(true) == Gate::Pending;
        check("gate/refused_turns_the_switch_off_and_a_later_install_clears_it", ok);

        Drops i = installedOn();
        int k = 0;
        ok = i.GateNow(false) == Gate::NoDetection && i.BeginCall(false, 0, true, ArgKind::Undefined, 0, k) == CallAction::PassGate
            && i.GateNow(true) == Gate::Pending;
        check("gate/no_detection_passes", ok);

        // A roll already in progress passes and is counted.
        ok = i.BeginCall(true, 1, true, ArgKind::Undefined, 0, k) == CallAction::PassNested
            && i.BeginCall(true, 0, true, ArgKind::Undefined, 0, k) == CallAction::Resolve && k == 5;
        DropStatFields f;
        i.FillStat(f, true);
        check("gate/nested_passes_and_is_counted", ok && f.nested == 1 && f.heroicCalls == 3, DropStatLine(f));

        // not-resolved: a list with no stand-in for any piece, or unreadable.
        FakeRepo none;
        none.add(10, 0, 47, 9);
        Drops n = installedOn();
        const PushPlan pn = n.PlanFor(ResolveStandIns(5, { entry(10, 0, 47) }, none.fn()), kPlayerCopies);
        n.NotePushed(pn);
        ok = pn.entries.empty() && n.GateNow(true) == Gate::NotResolved && n.PushedCalls() == 0;
        FakeRepo all;
        const auto list = fullList(all);
        const PushPlan pa = n.PlanFor(ResolveStandIns(5, list, all.fn()), kPlayerCopies);
        n.NotePushed(pa);
        ok = ok && n.GateNow(true) == Gate::On && n.PushedCalls() == 1;
        n.NoteListUnreadable(5);
        ok = ok && n.GateNow(true) == Gate::NotResolved;
        check("gate/not_resolved_when_no_stand_in_or_unreadable", ok);

        // A refused rewrite latches that piece off; the next plan leaves it out.
        Drops l = installedOn();
        FakeRepo lr;
        const auto llist = fullList(lr);
        const Resolution res = ResolveStandIns(5, llist, lr.fn());
        l.NoteRewriteRefused(Piece::Boots);
        const PushPlan pl = l.PlanFor(res, kPlayerCopies);
        ok = l.Latched(Piece::Boots) && !l.Latched(Piece::Helmet) && pl.entries.size() == 3;
        for (const PushEntry& e : pl.entries)
            if (e.piece == Piece::Boots) ok = false;
        check("gate/refused_rewrite_latches_the_piece", ok);
    }

    // ---- art_ready: no roll builds a piece before the ground install was tried --------
    {
        bool ok = ArtReady(false, 8, false) && ArtReady(true, 0, false) && ArtReady(true, 8, true)
            && !ArtReady(true, 8, false) && !ArtReady(true, 1, false) && ArtReady(false, 0, true);
        check("art_ready/not_armed_no_icon_or_ground_tried", ok);

        Drops d = installedOn();
        int k = 0;
        const CallAction a = d.BeginCall(true, 0, ArtReady(true, 8, false), ArgKind::Undefined, 0, k);
        const CallAction b = d.BeginCall(true, 0, ArtReady(true, 8, true), ArgKind::Undefined, 0, k);
        DropStatFields f;
        d.FillStat(f, true);
        check("art_ready/a_call_not_art_ready_pushes_nothing_and_is_counted",
              a == CallAction::PassArtWait && b == CallAction::Resolve && f.artWait == 1 && f.pushedCalls == 0, DropStatLine(f));

        // A drops switch-on arms the set's art flag (one flag, either switch).
        M1s::Bonus set;
        Drops sw;
        SetSwitch(sw, set, true);
        const bool armed = set.ArtArmed() && sw.Enabled() && !set.Enabled();
        SetSwitch(sw, set, false);
        check("art_ready/drops_switch_arms_the_art", armed && set.ArtArmed() && !sw.Enabled()
              && !ArtReady(set.ArtArmed(), 8, false));
    }

    // ---- stat: the `mark1set drops:` line and the reply ---------------------------------
    {
        const std::string fresh = DropStatLine(DropStatFields{});
        const std::string expected =
            "mark1set drops: drops=off install=not-armed gate=off hook=not-installed list=- heroic-calls=0"
            " pushed-calls=0 nested=0 art-wait=0 helmet=0/0 body=0/0 boots=0/0 gloves=0/0 injected=0 refused=0"
            " anomalies=0 standin-helmet=none standin-body=none standin-boots=none standin-gloves=none"
            " list-refused=0 unread=0 kill-gate=off kill-hook=not-installed kill-calls=0 kill-pushed=0 kill-nested=0"
            " kill-helmet=0/0 kill-body=0/0 kill-boots=0/0 kill-gloves=0/0 kill-standin-helmet=none"
            " kill-standin-body=none kill-standin-boots=none kill-standin-gloves=none draw-gate=off draw-hook=not-installed"
            " tier-hook=not-installed draw-calls=0 draw-pushed=0 draw-nested=0 tier-unread=0 draw-helmet=0/0 draw-body=0/0"
            " draw-boots=0/0 draw-gloves=0/0 draw-standin-helmet=none draw-standin-body=none draw-standin-boots=none"
            " draw-standin-gloves=none";
        check("stat/fresh_line_exact", fresh == expected, fresh);
        check("stat/fresh_line_begins_mark1set_drops", startsWith(fresh, "mark1set drops: drops=off"));
        Drops fd;
        DropStatFields ff;
        fd.FillStat(ff, true);
        check("stat/fresh_session_fills_the_fresh_line", DropStatLine(ff) == expected, DropStatLine(ff));
        check("stat/no_double_space", fresh.find("  ") == std::string::npos);

        const std::vector<std::string> names = fieldNames(fresh);
        const std::vector<std::string> order = { "drops", "install", "gate", "hook", "list", "heroic-calls", "pushed-calls",
            "nested", "art-wait", "helmet", "body", "boots", "gloves", "injected", "refused", "anomalies",
            "standin-helmet", "standin-body", "standin-boots", "standin-gloves", "list-refused", "unread",
            "kill-gate", "kill-hook", "kill-calls", "kill-pushed", "kill-nested", "kill-helmet", "kill-body",
            "kill-boots", "kill-gloves", "kill-standin-helmet", "kill-standin-body", "kill-standin-boots",
            "kill-standin-gloves", "draw-gate", "draw-hook", "tier-hook", "draw-calls", "draw-pushed", "draw-nested",
            "tier-unread", "draw-helmet", "draw-body", "draw-boots", "draw-gloves", "draw-standin-helmet",
            "draw-standin-body", "draw-standin-boots", "draw-standin-gloves" };
        check("stat/named_fields_in_order", names == order);

        // Filled: a session that pushed, hit, built and refused.
        Drops d = installedOn();
        FakeRepo repo;
        const auto list = fullList(repo);
        int k = 0;
        d.BeginCall(true, 0, true, ArgKind::Undefined, 0, k);
        const PushPlan plan = d.PlanFor(ResolveStandIns(k, list, repo.fn()), 2);
        d.NotePushed(plan);
        d.NoteOurs(Piece::Helmet);
        d.NoteBuilt(Piece::Helmet);
        d.NoteOurs(Piece::Gloves);
        d.NoteAnomaly();
        d.NoteRewriteRefused(Piece::Gloves);
        DropStatFields f;
        d.FillStat(f, true);
        f.hook = "both";
        const std::string line = DropStatLine(f);
        const bool ok = field(line, "drops") == "on" && field(line, "install") == "installed" && field(line, "gate") == "on"
            && field(line, "hook") == "both" && field(line, "list") == "5" && field(line, "heroic-calls") == "1"
            && field(line, "pushed-calls") == "1" && field(line, "injected") == "8" && field(line, "helmet") == "1/1"
            && field(line, "gloves") == "1/0" && field(line, "body") == "0/0" && field(line, "refused") == "1"
            && field(line, "anomalies") == "1" && field(line, "standin-helmet") == "3:17@9x1"
            && field(line, "standin-gloves") == "0:5@9x1" && field(line, "unread") == "0";
        check("stat/filled_fields", ok, line);
        check("stat/stand_in_text", StandInText(StandIn{}) == "none"
              && StandInText(StandIn{ true, Triple{ 2, 1, 8 }, 6, 3 }) == "1:8@6x3");

        Drops r;
        r.SetEnabled(true);
        check("stat/reply_names_the_switch_the_install_and_the_gate",
              DropsReply(r, true) == "mark1set: drops=on install=waiting-for-character gate=off kill-gate=off draw-gate=off",
              DropsReply(r, true));
        Drops ri = installedOn();
        Drops rf;
        rf.SetEnabled(true);
        rf.NoteInstall(false, false);
        check("stat/reply_installed_and_refused",
              DropsReply(ri, true) == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending"
              && DropsReply(ri, false) == "mark1set: drops=on install=installed gate=no-detection kill-gate=no-detection draw-gate=no-detection"
              && DropsReply(rf, true) == "mark1set: drops=off install=refused gate=refused kill-gate=refused draw-gate=refused",
              DropsReply(ri, true) + " | " + DropsReply(rf, true));
        check("stat/gate_names", GateName(Gate::Off) == "off" && GateName(Gate::On) == "on"
              && GateName(Gate::Refused) == "refused" && GateName(Gate::NotResolved) == "not-resolved"
              && GateName(Gate::NoDetection) == "no-detection" && GateName(Gate::Pending) == "pending");
    }

    // ---- kill: the ordinary unique roll -------------------------------------------------
    {
        // Baseline: the switch off is the game's own roll; the call is counted.
        Drops d;
        const CallAction a = d.BeginKillCall(true, 0, 0, true);
        DropStatFields f;
        d.FillStat(f, true);
        Drops off = installedOn();
        off.SetEnabled(false);
        const CallAction b = off.BeginKillCall(true, 0, 0, true);
        check("kill/baseline_switch_off_passes_and_only_counts_the_call",
              a == CallAction::PassOff && b == CallAction::PassOff && off.Installed() && f.killCalls == 1
              && f.killPushedCalls == 0 && f.killNested == 0 && f.artWait == 0 && f.injected == 0
              && f.heroicCalls == 0 && off.KillCalls() == 1 && off.KillPushedCalls() == 0, DropStatLine(f));

        // The ordinary roll's filter: below 7 and not hidden.
        bool ok = kOrdinaryRarityBelow == 7 && OrdinaryKeeps(6, false) && OrdinaryKeeps(0, false) && OrdinaryKeeps(2, false)
            && !OrdinaryKeeps(7, false) && !OrdinaryKeeps(9, false) && !OrdinaryKeeps(10, false) && !OrdinaryKeeps(6, true);
        check("kill/the_ordinary_rolls_filter", ok);

        // The equal-count rule over the six lists.
        FakeRepo repo;
        const KillLists lists = killLists(repo);
        const KillResolution r = ResolveKillStandIns(lists, repo.fn());
        ok = isTriple(r.standIns[0], 0, 3, 17) && r.standIns[0].n == 0 && r.standIns[0].rarity == 6
            && isTriple(r.standIns[1], 1, 2, 40) && r.standIns[1].n == 1
            && isTriple(r.standIns[2], 2, 4, 4) && r.standIns[2].n == 2 && r.standIns[2].rarity == 5
            && !r.standIns[3].resolved && r.AnyResolved() && r.unread == 0;
        check("kill/standin_equal_count_rule", ok,
              StandInText(r.standIns[0]) + " " + StandInText(r.standIns[1]) + " " + StandInText(r.standIns[2]));
        ok = r.kept[0].keptTriples == 2 && r.qualifying[0] == 2 && r.kept[0].keptEntries == 5
            && r.kept[1].keptTriples == 2 && r.qualifying[1] == 1 && r.kept[1].keptEntries == 11
            && r.kept[2].keptTriples == 2 && r.qualifying[2] == 2 && r.kept[3].keptTriples == 0 && r.qualifying[3] == 0;
        for (int k = 0; k < kKillListCount; ++k)
            if (r.counts[0][static_cast<size_t>(k)] != 0 || r.counts[1][static_cast<size_t>(k)] != 1
                || r.counts[2][static_cast<size_t>(k)] != 2) ok = false;
        check("kill/standin_unequal_counts_never_qualify", ok);
        ok = r.lengths[0] == 10 && r.lengths[1] == 9 && r.lengths[4] == 9 && r.lengths[5] == 5;
        check("kill/standin_lengths_of_all_six_lists", ok);
        // Each distinct triple of a piece class asked once, the charm never.
        ok = repo.calls == 9;
        for (const Triple& t : repo.asked)
            if (t.t == 10) ok = false;
        check("kill/standin_each_distinct_triple_asked_once", ok, std::to_string(repo.calls));

        // The smaller n wins over a lower (sub, b); n 0 over anything.
        FakeRepo s;
        s.add(1, 0, 0, 6);
        s.add(1, 9, 9, 6);
        KillLists sl;
        for (int k = 0; k < kKillListCount; ++k)
            sl[static_cast<size_t>(k)] = { entry(1, 0, 0), entry(1, 0, 0), entry(1, 9, 9) };
        const KillResolution rs = ResolveKillStandIns(sl, s.fn());
        s.add(1, 5, 5, 6);
        sl[5] = { entry(1, 5, 5) };
        const KillResolution rz = ResolveKillStandIns(sl, s.fn());
        check("kill/standin_smaller_n_then_count_zero_preferred", isTriple(rs.standIns[1], 1, 9, 9) && rs.standIns[1].n == 1
              && isTriple(rz.standIns[1], 1, 5, 5) && rz.standIns[1].n == 0 && rz.qualifying[1] == 3);

        // The Heroic roll's stand-ins are refused here: a class with only those has none.
        FakeRepo h;
        h.add(4, 0, 5, 9);
        KillLists hl;
        hl[5] = { entry(4, 0, 5) };
        for (int k = 0; k < kKillListCount; ++k) hl[static_cast<size_t>(k)] = { entry(4, 0, 5) };
        const KillResolution rh = ResolveKillStandIns(hl, h.fn());
        check("kill/standin_a_heroic_triple_never_stands_in", !rh.AnyResolved() && rh.kept[3].keptTriples == 0);

        // An unreadable verdict is counted and never a stand-in.
        FakeRepo u;
        u.add(0, 5, 5, 6);
        KillLists ul;
        ul[5] = { entry(0, 1, 1), entry(0, 5, 5), entry(0, 0.5, 1) };
        const KillResolution ru = ResolveKillStandIns(ul, u.fn());
        const KillResolution rnull = ResolveKillStandIns(ul, VerdictFn{});
        check("kill/standin_unreadable_counted_and_never_a_stand_in", ru.unread == 1 && isTriple(ru.standIns[0], 0, 5, 5)
              && ru.standIns[0].n == 0 && u.calls == 2 && rnull.unread == 2 && !rnull.AnyResolved());

        // The push: each resolved piece's copies onto every one of [0..4].
        bool lists04 = kKillListCount == 5;
        for (int k = 0; k < kKillListCount; ++k)
            if (kKillLists[k] != k) lists04 = false;
        const KillPushPlan p1 = PlanKillPush(r, kPlayerCopies, kNoLatch);
        ok = lists04 && p1.entries.size() == 3 && p1.PerListCount() == 3 && p1.EntryCount() == 15;
        for (const PushEntry& e : p1.entries)
            if (e.piece == Piece::Gloves || e.copies != 1) ok = false;
        ok = ok && p1.entries[0].n == 0 && p1.entries[1].n == 1 && p1.entries[2].n == 2;
        check("kill/push_onto_each_of_the_five_lists", ok, std::to_string(p1.EntryCount()));
        std::array<bool, kPieceCount> latch{};
        latch[1] = true;
        const KillPushPlan p2 = PlanKillPush(r, 2, latch);
        ok = p2.entries.size() == 2 && p2.EntryCount() == 20 && PlanKillPush(r, 0, kNoLatch).entries.empty()
            && PlanKillPush(r, -1, kNoLatch).EntryCount() == 0 && PlanKillPush(r, 1000, kNoLatch).entries[0].copies == kMaxCopies;
        for (const PushEntry& e : p2.entries)
            if (e.piece == Piece::Body || e.copies != 2) ok = false;
        check("kill/push_copies_bounded_and_latched_left_out", ok);

        // The coin c / (n + c), n the common count.
        Piece p = Piece::Helmet;
        ok = AttributeHit(p1, 0, 3, 17, 0.999, p) && p == Piece::Helmet          // n 0: every hit
            && AttributeHit(p1, 1, 2, 40, 0.4999, p) && p == Piece::Body          // n 1: half
            && !AttributeHit(p1, 1, 2, 40, 0.5, p)
            && AttributeHit(p1, 2, 4, 4, 0.33, p) && p == Piece::Boots           // n 2: a third
            && !AttributeHit(p1, 2, 4, 4, 0.34, p)
            && !AttributeHit(p1, 0, 1, 2, 0.0, p)                                 // on the lists, not the stand-in
            && !AttributeHit(p1, 2, 1, 8, 0.0, p)                                 // the Heroic roll's stand-in
            && !AttributeHit(p1, 4, 0, 5, 0.0, p) && !AttributeHit(KillPushPlan{}, 0, 3, 17, 0.0, p);
        check("kill/coin_is_c_over_n_plus_c_with_the_common_count", ok);

        // Resolved again when any of [0..5]'s id or length changed.
        KillStandInCache cache;
        const KillListKeys keys = keysOf(lists);
        ok = cache.Stale(keys) && cache.Find() == nullptr;
        cache.Store(keys, r);
        ok = ok && !cache.Stale(keys) && cache.Find() != nullptr && cache.Find()->standIns[0].triple.b == 17;
        for (size_t k = 0; k < keys.size(); ++k) {
            KillListKeys id = keys;
            ++id[k].id;
            KillListKeys len = keys;
            ++len[k].length;
            if (!cache.Stale(id) || !cache.Stale(len)) ok = false;
        }
        check("kill/cache_resolves_again_when_any_list_changes", ok);

        // A call nested in a roll in progress passes, counted: #74's or the
        // shared detour's own.
        Drops n = installedOn();
        ok = n.BeginKillCall(true, 1, 0, true) == CallAction::PassNested
            && n.BeginKillCall(true, 0, 1, true) == CallAction::PassNested
            && n.BeginKillCall(true, 0, 0, true) == CallAction::Resolve;
        DropStatFields fn;
        n.FillStat(fn, true);
        check("kill/nested_passes_and_is_counted", ok && fn.killNested == 2 && fn.killCalls == 3 && fn.nested == 0
              && fn.heroicCalls == 0, DropStatLine(fn));

        // #74's detection not inline: the game's own roll.
        Drops g = installedOn();
        check("kill/no_detection_passes", g.BeginKillCall(false, 0, 0, true) == CallAction::PassGate
              && g.KillGateNow(false) == Gate::NoDetection);

        // The art-ready rule is the Heroic roll's; `art-wait=` counts both.
        Drops w = installedOn();
        int k = 0;
        const CallAction wa = w.BeginKillCall(true, 0, 0, ArtReady(true, 8, false));
        const CallAction wb = w.BeginKillCall(true, 0, 0, ArtReady(true, 8, true));
        const CallAction wc = w.BeginCall(true, 0, ArtReady(true, 8, false), ArgKind::Undefined, 0, k);
        DropStatFields fw;
        w.FillStat(fw, true);
        check("kill/art_ready_applies_as_on_the_heroic_roll", wa == CallAction::PassArtWait && wb == CallAction::Resolve
              && wc == CallAction::PassArtWait && fw.artWait == 2 && fw.killPushedCalls == 0, DropStatLine(fw));

        // not-resolved: no kill stand-in for any piece, or the lists unreadable.
        FakeRepo none;
        none.add(10, 0, 47, 6);
        KillLists nl;
        for (int i = 0; i < kKillListCount; ++i) nl[static_cast<size_t>(i)] = { entry(10, 0, 47) };
        Drops nr = installedOn();
        const KillPushPlan pn = nr.PlanKillFor(ResolveKillStandIns(nl, none.fn()), kPlayerCopies);
        nr.NoteKillPushed(pn);
        ok = pn.entries.empty() && nr.KillGateNow(true) == Gate::NotResolved && nr.KillPushedCalls() == 0;
        FakeRepo all;
        const KillPushPlan pa = nr.PlanKillFor(ResolveKillStandIns(killLists(all), all.fn()), kPlayerCopies);
        nr.NoteKillPushed(pa);
        ok = ok && nr.KillGateNow(true) == Gate::On && nr.KillPushedCalls() == 1 && nr.GateNow(true) == Gate::Pending;
        nr.NoteKillListsUnreadable();
        ok = ok && nr.KillGateNow(true) == Gate::NotResolved;
        check("kill/not_resolved_when_no_stand_in_or_unreadable", ok);

        // A refused rewrite latches the piece off on both rolls.
        Drops l = installedOn();
        l.NoteRewriteRefused(Piece::Boots);
        const KillPushPlan pl = l.PlanKillFor(r, kPlayerCopies);
        ok = pl.entries.size() == 2;
        for (const PushEntry& e : pl.entries)
            if (e.piece == Piece::Boots) ok = false;
        check("kill/refused_rewrite_latches_the_piece_on_both_rolls", ok);

        // The stat line's kill fields, filled: the tallies by roll, the push
        // counted in `injected=`, and the stand-ins.
        Drops t = installedOn();
        t.BeginKillCall(true, 0, 0, true);
        const KillPushPlan pt = t.PlanKillFor(r, kPlayerCopies);
        t.NoteKillPushed(pt);
        t.NoteOurs(Piece::Helmet, Roll::Kill);
        t.NoteBuilt(Piece::Helmet, Roll::Kill);
        t.NoteOurs(Piece::Boots, Roll::Kill);
        t.NoteOurs(Piece::Body);
        DropStatFields ft;
        t.FillStat(ft, true);
        ft.hook = "both";
        ft.killHook = "detoured";
        const std::string line = DropStatLine(ft);
        ok = field(line, "kill-gate") == "on" && field(line, "kill-hook") == "detoured" && field(line, "kill-calls") == "1"
            && field(line, "kill-pushed") == "1" && field(line, "kill-nested") == "0" && field(line, "kill-helmet") == "1/1"
            && field(line, "kill-boots") == "1/0" && field(line, "kill-body") == "0/0" && field(line, "body") == "1/0"
            && field(line, "helmet") == "0/0" && field(line, "injected") == "15" && field(line, "pushed-calls") == "0"
            && field(line, "kill-standin-helmet") == "3:17@6x0" && field(line, "kill-standin-body") == "2:40@6x1"
            && field(line, "kill-standin-boots") == "4:4@5x2" && field(line, "kill-standin-gloves") == "none"
            && field(line, "gate") == "pending" && field(line, "heroic-calls") == "0";
        check("kill/stat_fields_filled", ok, line);
    }

    // ---- pending: each roll's gate and stand-ins before its first resolving call ----
    {
        // Switched on and installed: both rolls pending, every stand-in field pending.
        Drops d = installedOn();
        DropStatFields f;
        d.FillStat(f, true);
        std::string line = DropStatLine(f);
        bool ok = field(line, "gate") == "pending" && field(line, "kill-gate") == "pending"
            && field(line, "draw-gate") == "pending";
        for (const PieceRow& row : M1s::kPieces)
            if (field(line, "standin-" + std::string(row.key)) != "pending"
                || field(line, "kill-standin-" + std::string(row.key)) != "pending"
                || field(line, "draw-standin-" + std::string(row.key)) != "pending") ok = false;
        check("pending/both_gates_and_stand_ins_pending_until_the_first_roll", ok
              && DropsReply(d, true) == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending", line);

        // The Heroic roll resolves first: on, with its stand-ins; the kill route stays pending.
        FakeRepo hr;
        const auto hlist = fullList(hr);
        const PushPlan ph = d.PlanFor(ResolveStandIns(5, hlist, hr.fn()), kPlayerCopies);
        d.NotePushed(ph);
        DropStatFields fh;
        d.FillStat(fh, true);
        line = DropStatLine(fh);
        ok = field(line, "gate") == "on" && field(line, "standin-helmet") == "3:17@9x1"
            && field(line, "standin-gloves") == "0:5@9x1" && field(line, "kill-gate") == "pending"
            && field(line, "kill-standin-helmet") == "pending" && field(line, "kill-standin-gloves") == "pending";
        check("pending/heroic_resolves_to_on_and_the_kill_route_stays_pending", ok, line);

        // Then the kill route: on, with its stand-ins; a class with none reads none.
        FakeRepo kr;
        const KillPushPlan pk = d.PlanKillFor(ResolveKillStandIns(killLists(kr), kr.fn()), kPlayerCopies);
        d.NoteKillPushed(pk);
        DropStatFields fk;
        d.FillStat(fk, true);
        line = DropStatLine(fk);
        ok = field(line, "kill-gate") == "on" && field(line, "kill-standin-helmet") == "3:17@6x0"
            && field(line, "kill-standin-gloves") == "none" && field(line, "gate") == "on"
            && DropsReply(d, true) == "mark1set: drops=on install=installed gate=on kill-gate=on draw-gate=pending";
        check("pending/kill_resolves_to_on_and_a_class_with_none_reads_none", ok, line);

        // The kill route first, on its own, and a Heroic class with none.
        Drops k = installedOn();
        FakeRepo kr2;
        k.PlanKillFor(ResolveKillStandIns(killLists(kr2), kr2.fn()), kPlayerCopies);
        FakeRepo c;
        c.add(0, 3, 17, 9);
        k.PlanFor(ResolveStandIns(5, { entry(0, 3, 17) }, c.fn()), kPlayerCopies);
        DropStatFields fc;
        k.FillStat(fc, true);
        line = DropStatLine(fc);
        ok = field(line, "gate") == "on" && field(line, "standin-helmet") == "3:17@9x1" && field(line, "standin-body") == "none"
            && field(line, "standin-gloves") == "none" && field(line, "kill-gate") == "on";
        check("pending/a_roll_resolved_with_none_for_a_class_reads_none", ok, line);

        // Not pending without the switch, the install or the detection: the
        // fresh session's line, the armed reply, no-detection and refused.
        Drops fresh;
        Drops waiting;
        waiting.SetEnabled(true);
        Drops nodet = installedOn();
        Drops refused;
        refused.SetEnabled(true);
        refused.NoteInstall(false, false);
        DropStatFields fw;
        waiting.FillStat(fw, true);
        DropStatFields fnd;
        nodet.FillStat(fnd, false);
        DropStatFields fr;
        refused.FillStat(fr, true);
        ok = fresh.GateNow(true) == Gate::Off && fresh.KillGateNow(true) == Gate::Off
            && field(DropStatLine(fw), "standin-helmet") == "none" && field(DropStatLine(fw), "kill-standin-helmet") == "none"
            && field(DropStatLine(fw), "kill-gate") == "off"
            && field(DropStatLine(fnd), "gate") == "no-detection" && field(DropStatLine(fnd), "standin-body") == "none"
            && field(DropStatLine(fnd), "kill-standin-body") == "none"
            && field(DropStatLine(fr), "kill-gate") == "refused" && field(DropStatLine(fr), "kill-standin-boots") == "none";
        check("pending/not_pending_while_off_waiting_no_detection_or_refused", ok);

        // A list that could not be read before any roll resolved: not-resolved, not pending.
        Drops u = installedOn();
        u.NoteListUnreadable(5);
        u.NoteKillListsUnreadable();
        DropStatFields fu;
        u.FillStat(fu, true);
        line = DropStatLine(fu);
        ok = field(line, "gate") == "not-resolved" && field(line, "kill-gate") == "not-resolved"
            && field(line, "standin-helmet") == "none" && field(line, "kill-standin-helmet") == "none";
        check("pending/unreadable_before_the_first_roll_is_not_resolved", ok, line);

        // Switched off after a roll resolved: off, and the stand-ins as last resolved.
        d.SetEnabled(false);
        DropStatFields fo;
        d.FillStat(fo, true);
        line = DropStatLine(fo);
        check("pending/switched_off_after_a_roll_reads_off", field(line, "gate") == "off" && field(line, "kill-gate") == "off"
              && field(line, "standin-helmet") == "3:17@9x1" && field(line, "kill-standin-helmet") == "3:17@6x0", line);
    }

    // ---- draw: DropItem's own unique draw, the kill draw ------------------------------
    {
        // Baseline: the switch off is the game's own draw. The call is counted and
        // nothing else: no push, no hit, no nested roll set aside.
        Drops d;
        Piece p = Piece::Helmet;
        const CallAction a = d.BeginDrawCall(true, 0, 0, true);
        d.NoteDrawTier(true, 3);
        const bool asideOff = d.SetDrawAside();
        const DrawHit hOff = d.SettleDrawHit(0, 3, 17, 0.0, p);
        const bool takeOff = d.TakeDrawRewrite(p);
        const bool restoreOff = d.RestoreDraw();
        const bool endOff = d.EndDrawCall();
        DropStatFields f;
        d.FillStat(f, true);
        Drops off = installedOn();
        off.SetEnabled(false);
        const CallAction b = off.BeginDrawCall(true, 0, 0, true);
        off.EndDrawCall();
        check("draw/baseline_switch_off_passes_and_only_counts_the_call",
              a == CallAction::PassOff && !asideOff && hOff == DrawHit::NotInScope && !takeOff && !restoreOff && !endOff
              && !d.DrawPushing() && d.DrawDepth() == 0 && f.drawCalls == 1 && f.drawPushedCalls == 0 && f.drawNested == 0
              && f.tierUnread == 0 && f.injected == 0 && f.artWait == 0 && f.drawGate == Gate::Off && f.heroicCalls == 0
              && f.killCalls == 0 && b == CallAction::PassOff && off.Installed() && off.DrawCalls() == 1
              && off.DrawPushedCalls() == 0 && off.DrawDepth() == 0, DropStatLine(f));

        // Baseline: a call that pushed nothing (no stand-in anywhere) leaves a
        // nested roll untouched and settles no hit.
        FakeDrawRepo none;
        none.add(4, 0, 5, 9, 5);
        KillLists nl;
        for (auto& l : nl) l = { entry(4, 0, 5) };
        Drops n = installedOn();
        const CallAction na = n.BeginDrawCall(true, 0, 0, true);
        const DrawPlan np = n.PlanDrawFor(ResolveDrawStandIns(nl, none.fn()), kPlayerCopies);
        n.NoteDrawPushed(np);
        const bool nAside = n.SetDrawAside();
        const bool nRestore = n.RestoreDraw();
        check("draw/baseline_a_nested_roll_with_nothing_pushed_passes_untouched",
              na == CallAction::Resolve && np.entries.empty() && !n.DrawPushing() && !nAside && !nRestore
              && n.SettleDrawHit(4, 0, 5, 0.0, p) == DrawHit::NotInScope && n.DrawNested() == 0 && n.DrawPushedCalls() == 0
              && !n.EndDrawCall() && n.DrawGateNow(true) == Gate::NotResolved);

        // Baseline: nothing pushed attributes nothing, whatever the k, the triple
        // and the coin.
        bool ok = PlanDrawPush(DrawResolution{}, kPlayerCopies, kNoLatch).entries.empty();
        for (int k = -1; k <= kDrawListCount; ++k)
            for (double u : { 0.0, 0.5, 0.999 })
                if (AttributeDrawHit(DrawPlan{}, k, 0, 3, 17, u, p) != DrawHit::GameOwn) ok = false;
        check("draw/baseline_nothing_pushed_attributes_nothing", ok);

        // A call begun inside a roll in progress (#74's or the Heroic hook's, the
        // shared DropUniqueItems detour's) passes, counted; so does one begun
        // inside another DropItem call.
        Drops r = installedOn();
        ok = r.BeginDrawCall(true, 1, 0, true) == CallAction::PassNested;
        r.EndDrawCall();
        ok = ok && r.BeginDrawCall(true, 0, 1, true) == CallAction::PassNested;
        r.EndDrawCall();
        ok = ok && r.BeginDrawCall(true, 0, 0, true) == CallAction::Resolve
            && r.BeginDrawCall(true, 0, 0, true) == CallAction::PassNested && r.DrawDepth() == 2;
        r.EndDrawCall();
        r.EndDrawCall();
        check("draw/baseline_a_call_begun_inside_a_roll_passes_and_is_counted",
              ok && r.DrawNested() == 3 && r.DrawCalls() == 4 && r.DrawDepth() == 0 && !r.DrawPushing());

        // #74's detection not inline, or the art not ready: the game's own draw.
        Drops g = installedOn();
        const CallAction ga = g.BeginDrawCall(false, 0, 0, true);
        g.EndDrawCall();
        const CallAction gb = g.BeginDrawCall(true, 0, 0, ArtReady(true, 8, false));
        g.EndDrawCall();
        DropStatFields fg;
        g.FillStat(fg, true);
        check("draw/target_no_detection_or_art_wait_passes", ga == CallAction::PassGate && gb == CallAction::PassArtWait
              && g.DrawGateNow(false) == Gate::NoDetection && fg.artWait == 1 && fg.drawPushedCalls == 0, DropStatLine(fg));

        // The draw's filter: rarity 6, fields 40 and 41 clear; SS preferred.
        DrawRead keep;
        keep.rarity = 6;
        DrawRead heroic = keep;
        heroic.rarity = 9;
        DrawRead f40 = keep;
        f40.f40 = true;
        DrawRead f41 = keep;
        f41.f41 = true;
        DrawRead below = keep;
        below.rarity = 5;
        check("draw/target_the_draws_filter", DrawKeeps(keep) && !DrawKeeps(heroic) && !DrawKeeps(f40) && !DrawKeeps(f41)
              && !DrawKeeps(below) && kDrawKeptRarity == 6 && kDrawSsTier == 5 && kDrawPrefersSs && kDrawListCount == 6
              && kDrawNoTier == -1);

        // The stand-in rule over the six lists.
        FakeDrawRepo repo;
        const KillLists lists = drawLists(repo);
        const DrawResolution res = ResolveDrawStandIns(lists, repo.fn());
        check("draw/target_standin_ss_preferred_over_a_smaller_count",
              isDraw(res.standIns[0], 0, 3, 17) && res.standIns[0].tier == 5 && res.standIns[0].rarity == 6
              && res.standIns[0].counts == countsOf(2, 2, 2, 2, 2, 2) && res.candidates[0] == 2 && res.ss[0] == 1,
              DrawStandInText(res.standIns[0]));
        check("draw/target_standin_falls_back_below_ss_by_the_smaller_largest_count",
              isDraw(res.standIns[1], 1, 5, 5) && res.standIns[1].tier == 2 && res.standIns[1].counts == countsOf(1, 1, 1, 1, 1, 1)
              && res.candidates[1] == 2 && res.ss[1] == 0 && DrawStandInText(res.standIns[1]) == "5:5@6/2x1,1,1,1,1,1",
              DrawStandInText(res.standIns[1]));
        check("draw/target_standin_ties_by_the_lowest_sub_then_b",
              isDraw(res.standIns[2], 2, 4, 4) && res.standIns[2].counts == countsOf(0, 0, 0, 1, 0, 0) && res.candidates[2] == 2
              && res.ss[2] == 2, DrawStandInText(res.standIns[2]));
        check("draw/target_standin_fields_40_41_heroic_and_unread_never_stand_in",
              !res.standIns[3].resolved && res.candidates[3] == 0 && res.unread == 1);
        // A class with none: gloves push nothing, the others still do.
        const DrawPlan p1 = PlanDrawPush(res, kPlayerCopies, kNoLatch);
        ok = res.AnyResolved() && p1.entries.size() == 3 && p1.PerListCount() == 3 && p1.EntryCount() == 18;
        for (const DrawPushEntry& e : p1.entries)
            if (e.piece == Piece::Gloves || e.copies != 1) ok = false;
        check("draw/target_standin_a_class_with_none_leaves_the_others", ok, std::to_string(p1.EntryCount()));
        // Each distinct triple of a piece class asked once, the charm never; the
        // lengths of all six lists.
        ok = repo.calls == 11 && res.lengths[0] == 7 && res.lengths[1] == 5 && res.lengths[2] == 7 && res.lengths[3] == 8
            && res.lengths[4] == 7 && res.lengths[5] == 7;
        for (const Triple& t : repo.asked)
            if (t.t == 10) ok = false;
        check("draw/target_standin_each_distinct_triple_asked_once_over_six_lists", ok, std::to_string(repo.calls));
        // No reader: every triple unread, none chosen.
        const DrawResolution rnull = ResolveDrawStandIns(lists, DrawReadFn{});
        check("draw/target_standin_no_reader_is_unread", rnull.unread == 11 && !rnull.AnyResolved());

        // Resolved again when any of [0..5]'s id or length changed.
        DrawStandInCache cache;
        const KillListKeys keys = keysOf(lists);
        ok = cache.Stale(keys) && cache.Find() == nullptr;
        cache.Store(keys, res);
        ok = ok && !cache.Stale(keys) && cache.Find() != nullptr && cache.Find()->standIns[0].triple.b == 17;
        for (size_t k = 0; k < keys.size(); ++k) {
            KillListKeys id = keys;
            ++id[k].id;
            KillListKeys len = keys;
            ++len[k].length;
            if (!cache.Stale(id) || !cache.Stale(len)) ok = false;
        }
        check("draw/target_cache_resolves_again_when_any_list_changes", ok);

        // The push: copies onto each of [0..5], bounded, latched pieces left out.
        std::array<bool, kPieceCount> latch{};
        latch[1] = true;
        const DrawPlan p2 = PlanDrawPush(res, 2, latch);
        ok = p2.entries.size() == 2 && p2.EntryCount() == 24 && PlanDrawPush(res, 0, kNoLatch).entries.empty()
            && PlanDrawPush(res, -1, kNoLatch).EntryCount() == 0 && PlanDrawPush(res, 1000, kNoLatch).entries[0].copies == kMaxCopies;
        for (const DrawPushEntry& e : p2.entries)
            if (e.piece == Piece::Body || e.copies != 2) ok = false;
        check("draw/target_push_onto_each_of_the_six_lists_bounded_and_latched", ok);

        // The coin c / (n_k + c), for every k from 0 to 5; none for a k that
        // names no list.
        ok = true;
        for (int k = 0; k < kDrawListCount; ++k) {
            const double boots = k == 3 ? 0.5 : 1.0;
            if (DrawCoinShare(p1.entries[0], k) != 1.0 / 3.0 || DrawCoinShare(p1.entries[1], k) != 0.5
                || DrawCoinShare(p1.entries[2], k) != boots || DrawCoinShare(p2.entries[0], k) != 0.5) ok = false;
            Piece q = Piece::Helmet;
            if (AttributeDrawHit(p1, k, 2, 4, 4, 0.49, q) != DrawHit::Ours || q != Piece::Boots) ok = false;
            if ((AttributeDrawHit(p1, k, 2, 4, 4, 0.5, q) == DrawHit::Ours) != (k != 3)) ok = false;
            if (AttributeDrawHit(p1, k, 0, 3, 17, 0.33, q) != DrawHit::Ours || q != Piece::Helmet) ok = false;
            if (AttributeDrawHit(p1, k, 0, 3, 17, 0.34, q) != DrawHit::GameOwn) ok = false;
        }
        ok = ok && DrawCoinShare(p1.entries[0], kDrawNoTier) == 0.0 && DrawCoinShare(p1.entries[0], 6) == 0.0
            && AttributeDrawHit(p1, 0, 2, 4, 6, 0.0, p) == DrawHit::GameOwn     // on the lists, not the stand-in
            && AttributeDrawHit(p1, 0, 0, 1, 2, 0.0, p) == DrawHit::GameOwn     // below SS, not chosen
            && AttributeDrawHit(p1, 0, 0, 3, 17.5, 0.0, p) == DrawHit::GameOwn
            && AttributeDrawHit(p1, 0, 0, 3, 17, 1.0, p) == DrawHit::GameOwn
            && AttributeDrawHit(p1, 0, 0, 3, 17, std::nan(""), p) == DrawHit::GameOwn;
        check("draw/target_coin_c_over_n_k_plus_c_for_every_k", ok);

        // A stand-in's hit with no tier recorded is refused and counted; a game
        // hit is not. A tier that names no list leaves the call with none.
        Drops t = pushingCall();
        ok = t.DrawPushing() && t.DrawTier() == kDrawNoTier
            && t.SettleDrawHit(0, 3, 17, 0.0, p) == DrawHit::TierUnread
            && t.SettleDrawHit(10, 0, 47, 0.0, p) == DrawHit::GameOwn && t.TierUnread() == 1 && !t.TakeDrawRewrite(p);
        t.NoteDrawTier(true, 2.5);
        ok = ok && t.DrawTier() == kDrawNoTier;
        t.NoteDrawTier(true, 6);
        ok = ok && t.DrawTier() == kDrawNoTier;
        t.NoteDrawTier(false, 3);
        ok = ok && t.DrawTier() == kDrawNoTier;
        t.NoteDrawTier(true, 4);
        ok = ok && t.DrawTier() == 4 && t.SettleDrawHit(0, 3, 17, 0.0, p) == DrawHit::Ours && p == Piece::Helmet
            && t.TierUnread() == 1;
        t.NoteDrawTier(true, -1);
        ok = ok && t.DrawTier() == kDrawNoTier && t.SettleDrawHit(0, 3, 17, 0.0, p) == DrawHit::TierUnread && t.TierUnread() == 2;
        check("draw/target_no_tier_recorded_is_refused_and_counted", ok);

        // Two hits in one call, each settled before the next: each rewritten
        // once, and a hit never rewritten does not leak into the next build.
        Drops two = pushingCall();
        two.NoteDrawTier(true, 0);
        Piece first = Piece::Helmet;
        Piece second = Piece::Helmet;
        ok = two.SettleDrawHit(2, 4, 4, 0.9, p) == DrawHit::Ours && two.TakeDrawRewrite(first) && first == Piece::Boots
            && !two.TakeDrawRewrite(first)
            && two.SettleDrawHit(0, 3, 17, 0.1, p) == DrawHit::Ours && two.TakeDrawRewrite(second) && second == Piece::Helmet;
        ok = ok && two.SettleDrawHit(1, 5, 5, 0.1, p) == DrawHit::Ours                // the body's, not rewritten
            && two.SettleDrawHit(10, 0, 47, 0.0, p) == DrawHit::GameOwn && !two.TakeDrawRewrite(p)
            && two.SettleDrawHit(1, 5, 5, 0.6, p) == DrawHit::GameOwn && !two.TakeDrawRewrite(p);   // lost on the coin
        DropStatFields f2;
        two.FillStat(f2, true);
        check("draw/target_two_hits_in_one_call_settled_apart", ok && f2.drawPieces[0].ours == 1 && f2.drawPieces[1].ours == 1
              && f2.drawPieces[2].ours == 1 && f2.drawPieces[3].ours == 0 && f2.drawPieces[0].built == 0, DropStatLine(f2));

        // A roll run inside the call sets its entries and its hit state aside;
        // both come back when that roll returns.
        Drops nest = pushingCall();
        nest.NoteDrawTier(true, 1);
        ok = nest.SettleDrawHit(0, 3, 17, 0.1, p) == DrawHit::Ours;          // waiting for its rewrite
        const bool outer = nest.SetDrawAside();
        const bool inner = nest.SetDrawAside();
        ok = ok && outer && !inner && nest.DrawAside() && nest.SettleDrawHit(0, 3, 17, 0.0, p) == DrawHit::NotInScope
            && !nest.TakeDrawRewrite(p) && nest.PushedDrawPlan() != nullptr && nest.PushedDrawPlan()->EntryCount() == 18;
        const bool innerBack = nest.RestoreDraw();
        const bool outerBack = nest.RestoreDraw();
        Piece back = Piece::Body;
        ok = ok && !innerBack && outerBack && !nest.DrawAside() && !nest.RestoreDraw() && nest.TakeDrawRewrite(back)
            && back == Piece::Helmet && nest.DrawNested() == 1 && nest.TierUnread() == 0;
        DropStatFields fn;
        nest.FillStat(fn, true);
        check("draw/target_a_nested_roll_sets_the_entries_and_the_hit_aside", ok && fn.drawPieces[0].ours == 1
              && fn.drawNested == 1, DropStatLine(fn));

        // The tier goes to the innermost DropItem call in progress; the inner
        // call's end leaves the pushing call's frame, its own ends it.
        Drops in = pushingCall();
        in.NoteDrawTier(true, 3);
        ok = in.BeginDrawCall(true, 0, 0, true) == CallAction::PassNested && in.DrawTier() == kDrawNoTier;
        in.NoteDrawTier(true, 5);
        ok = ok && in.DrawTier() == 5 && in.SettleDrawHit(2, 4, 4, 0.9, p) == DrawHit::Ours   // n_5 0: every hit
            && in.TakeDrawRewrite(p) && !in.EndDrawCall() && in.DrawPushing() && in.DrawTier() == 3
            && in.SettleDrawHit(2, 4, 4, 0.9, p) == DrawHit::GameOwn                              // n_3 1: a half
            && in.EndDrawCall() && !in.DrawPushing() && in.DrawDepth() == 0 && in.PushedDrawPlan() == nullptr
            && in.SettleDrawHit(2, 4, 4, 0.0, p) == DrawHit::NotInScope && !in.EndDrawCall();
        check("draw/target_the_tier_goes_to_the_innermost_call", ok);

        // The gate: pending until the first resolving call, then on, or
        // not-resolved with no stand-in or the lists unreadable.
        Drops gt = installedOn();
        ok = gt.DrawGateNow(true) == Gate::Pending && gt.GateNow(true) == Gate::Pending && gt.KillGateNow(true) == Gate::Pending;
        gt.PlanDrawFor(ResolveDrawStandIns(nl, none.fn()), kPlayerCopies);
        ok = ok && gt.DrawGateNow(true) == Gate::NotResolved;
        FakeDrawRepo gr;
        gt.PlanDrawFor(ResolveDrawStandIns(drawLists(gr), gr.fn()), kPlayerCopies);
        ok = ok && gt.DrawGateNow(true) == Gate::On && gt.KillGateNow(true) == Gate::Pending && gt.GateNow(true) == Gate::Pending;
        gt.NoteDrawListsUnreadable();
        ok = ok && gt.DrawGateNow(true) == Gate::NotResolved && gt.DrawGateNow(false) == Gate::NoDetection;
        gt.SetEnabled(false);
        ok = ok && gt.DrawGateNow(true) == Gate::Off;
        check("draw/target_gate_pending_then_on_or_not_resolved", ok);

        // A refused rewrite latches the piece off on the draw too.
        Drops lr = installedOn();
        lr.NoteRewriteRefused(Piece::Boots);
        const DrawPlan pl = lr.PlanDrawFor(res, kPlayerCopies);
        ok = pl.entries.size() == 2;
        for (const DrawPushEntry& e : pl.entries)
            if (e.piece == Piece::Boots) ok = false;
        check("draw/target_refused_rewrite_latches_the_piece_on_the_draw", ok);

        // The line's draw fields, filled: a call pushed, hit, built; `injected=`
        // counts the push; the other routes' fields stay theirs.
        Drops s = pushingCall();
        s.NoteDrawTier(true, 2);
        s.SettleDrawHit(0, 3, 17, 0.0, p);
        Piece built = Piece::Body;
        s.TakeDrawRewrite(built);
        s.NoteBuilt(built, Roll::Draw);
        s.SettleDrawHit(2, 4, 4, 0.0, p);
        s.SetDrawAside();
        s.RestoreDraw();
        s.EndDrawCall();
        s.BeginDrawCall(true, 1, 0, true);
        s.EndDrawCall();
        s.BeginDrawCall(true, 0, 0, true);
        s.SettleDrawHit(0, 3, 17, 0.0, p);   // after the call that pushed: not the draw's
        s.EndDrawCall();
        DropStatFields fs;
        s.FillStat(fs, true);
        fs.drawHook = "detoured";
        fs.tierHook = "detoured";
        const std::string line = DropStatLine(fs);
        ok = field(line, "draw-gate") == "on" && field(line, "draw-hook") == "detoured" && field(line, "tier-hook") == "detoured"
            && field(line, "draw-calls") == "3" && field(line, "draw-pushed") == "1" && field(line, "draw-nested") == "2"
            && field(line, "tier-unread") == "0" && field(line, "draw-helmet") == "1/1" && field(line, "draw-boots") == "1/0"
            && field(line, "draw-body") == "0/0" && field(line, "draw-gloves") == "0/0" && field(line, "injected") == "18"
            && field(line, "draw-standin-helmet") == "3:17@6/5x2,2,2,2,2,2"
            && field(line, "draw-standin-body") == "5:5@6/2x1,1,1,1,1,1"
            && field(line, "draw-standin-boots") == "4:4@6/5x0,0,0,1,0,0" && field(line, "draw-standin-gloves") == "none"
            && field(line, "helmet") == "0/0" && field(line, "kill-helmet") == "0/0" && field(line, "gate") == "pending"
            && field(line, "kill-gate") == "pending" && field(line, "pushed-calls") == "0" && field(line, "kill-pushed") == "0";
        check("draw/target_stat_fields_filled", ok, line);
        check("draw/target_stand_in_text", DrawStandInText(DrawStandIn{}) == "none"
              && DrawStandInField(Gate::Pending, res.standIns[0]) == "pending"
              && DrawStandInField(Gate::On, res.standIns[3]) == "none");
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
