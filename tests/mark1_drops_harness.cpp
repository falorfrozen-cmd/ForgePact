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
// fresh and filled, and the reply (stat/*).
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

static Drops installedOn()
{
    Drops d;
    d.SetEnabled(true);
    d.NoteInstall(true, false);
    return d;
}

static const std::array<bool, kPieceCount> kNoLatch{};

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
        ok = ok && r.InstallStateName() == "installed" && r.GateNow(true) == Gate::On;
        check("gate/refused_turns_the_switch_off_and_a_later_install_clears_it", ok);

        Drops i = installedOn();
        int k = 0;
        ok = i.GateNow(false) == Gate::NoDetection && i.BeginCall(false, 0, true, ArgKind::Undefined, 0, k) == CallAction::PassGate
            && i.GateNow(true) == Gate::On;
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
            " list-refused=0 unread=0";
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
            "standin-helmet", "standin-body", "standin-boots", "standin-gloves", "list-refused", "unread" };
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
              DropsReply(r, true) == "mark1set: drops=on install=waiting-for-character gate=off", DropsReply(r, true));
        Drops ri = installedOn();
        Drops rf;
        rf.SetEnabled(true);
        rf.NoteInstall(false, false);
        check("stat/reply_installed_and_refused",
              DropsReply(ri, true) == "mark1set: drops=on install=installed gate=on"
              && DropsReply(ri, false) == "mark1set: drops=on install=installed gate=no-detection"
              && DropsReply(rf, true) == "mark1set: drops=off install=refused gate=refused", DropsReply(rf, true));
        check("stat/gate_names", GateName(Gate::Off) == "off" && GateName(Gate::On) == "on"
              && GateName(Gate::Refused) == "refused" && GateName(Gate::NotResolved) == "not-resolved"
              && GateName(Gate::NoDetection) == "no-detection");
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
