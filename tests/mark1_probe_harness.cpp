// Behavioral regression harness for mark1probe's core
// (Mark1Probe.hpp, ForgePact #166 phase 1, research build only).
//
// The Python runner injects the REAL ForgePact::Mark1Probe header below. No
// game is touched: a hooked call is a lambda standing in for the trampoline,
// and the adapter's reads and writes go through FakeAccess, a table of member
// values, a position and a result that records every read and write.
//
// Baseline (labels baseline/*): with no lever on, every row's call, for the
// local player and for any other self, runs the original once, leaves its
// result unchanged and reads or writes nothing. Targets (labels lever/*):
// `set` and `scale` write before the original and restore after it, also when
// the original throws, only for the local player, once per nested call, and
// only within their frame budget; `move` scales a call's own displacement by
// 0.5 and by 3 and leaves a call that did not move the player alone; `stat`
// changes only a number returned for the matching stat id and the local
// player, on the dispatcher and on the id's own getter row (64, 66), and
// scales a number once when the dispatcher hands the id to the detoured
// getter; a second lever replaces the first. Walkable (walkable/*): the
// verdict line for each field's values, `unreadable` for every reading not
// made, each field's raw builtin answer, place_meeting read as a meeting
// (false or 0 free) and collision_line as an instance or noone, and `scan`'s
// point list. Parsers (parse/*): every form, accepted and
// refused. The lines: the bare status, `hook`, `show`, `args`, `watch`,
// `count` and `pos`, including pos's frame samples (a step no row made is an
// `outside` residual; fewer than two samples measure `none`).
//
// Phase 2's walk research (labels builtin/*, statlog/*, statpin/*, hold/*,
// base/*, and the new watch/, pos/ and parse/ ones): the builtin rows are rows
// (pass-through, `move`, `pos`), a builtin the runtime lacks is `not-found`;
// `statlog` prints each local answer for its id with the rows around it and
// writes nothing; `statpin` answers with the last local answer before it on
// both routes, once per call; `hold` writes before each outer local call and
// restores once, at its end, on `off` or before the next lever; the effect,
// pst array and builtin watch names parse back as targets; `pos` measures its
// modal step and frame time from the samples alone; `base`'s key, definition
// and line; and every lever reply names its start frame.
//
// The Mark 1 set's adapter (labels spawn/*): `spawn`'s pieces by word, each
// piece's definition and loader key from Mark1Set.hpp's own row, the form's
// parse, and its line placed, refused at the ground and not built, with the
// tier it read (info key 32) after `dressed=`, `?` when unread.
//
// The drops' forms (labels heroic/*, pool/*, copies/*): `heroic`'s per-call
// line (the tier, info key 32, after `rarity=`, `?` when unread) and its summary (rarities rising, the pieces by their own recognition),
// `pool`'s summary of one sub-list as the Heroic roll keeps it - its rarity
// histograms, the entries it draws past by why, and each piece's stand-in by
// Mark1Drops.hpp's rule, each distinct triple's verdict asked once - and each
// form's parse and refusals.
//
// Mod live procedure 6's lever (labels killgate/*): baseline, the lever off, every
// LoadDrops call is the game's own with nothing written or re-entered and only the
// counters moving, and a hook other than the inline detour refuses the lever; target, the
// game's own type-45 call raised and put back, the first call on a fresh array re-entered
// once at type 45 and put back, later calls on that array untouched, a throw putting the
// chance back and counting `threw=`, a nested call untouched; the stat line's exact text.
//
// Mod live procedure 7's watch (labels draws/*, pool/draw_*): baseline, the watch off,
// nothing is counted, no scope opens, a DropItem call is not taken and the lever pushes
// nothing, and a DropItem route other than the inline detour refuses the watch; target,
// each scope (draw, dropunique, angelic, heroic, outside) classes a build by the innermost
// one open, k, on-k and off-k follow the recorded tier, the lever's plan puts c of each
// kill stand-in on each of [0..5] and the removal takes exactly that off (the tail rule
// leaving a changed list as found, an add that throws taking the partial push off), the
// picks and pick-builds of a pushed stand-in, the lever's refusals, and the status and
// built lines' exact text; `pool draw`'s candidate rule and lines.
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// PRODUCTION_MARK1PROBE

using namespace ForgePact::Mark1Probe;

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

static std::string why(const std::string& args)
{
    Command c;
    std::string reason;
    return Parse(words(args), c, reason) ? std::string("ACCEPTED") : reason;
}

static Command parsed(const std::string& args)
{
    Command c;
    std::string reason;
    if (!Parse(words(args), c, reason)) c.form = Form::Status, c.frames = -1;
    return c;
}

// What the adapter hands the core around one call: the scope members the
// lever's target names, the local player's x and y, the call's stat-id
// argument and the call's result. Every read and write is logged, so a
// baseline can assert that nothing was touched.
struct FakeAccess {
    std::map<std::string, Value> members;
    double x = 100, y = 200;
    bool hasStatId = true;
    double statId = 64;
    bool resultIsNumber = true;
    double result = 0;
    int reads = 0;
    int writes = 0;
    int positionReads = 0;
    int positionWrites = 0;
    int statReads = 0;
    int resultWrites = 0;
    std::vector<std::string> log;

    bool Read(const Target& t, Value& out)
    {
        ++reads;
        const auto it = members.find(TargetText(t));
        if (it == members.end()) return false;
        out = it->second;
        return true;
    }
    bool Write(const Target& t, const Value& v)
    {
        ++writes;
        log.push_back("write " + TargetText(t) + "=" + ValueText(v));
        members[TargetText(t)] = v;
        return true;
    }
    bool Position(double& px, double& py)
    {
        ++positionReads;
        px = x;
        py = y;
        return true;
    }
    bool SetPosition(double px, double py)
    {
        ++positionWrites;
        x = px;
        y = py;
        return true;
    }
    bool StatId(double& id)
    {
        ++statReads;
        id = statId;
        return hasStatId;
    }
    bool Result(double& n)
    {
        n = result;
        return resultIsNumber;
    }
    std::string ResultKind() { return resultKind; }
    std::string resultKind = "string";
    void SetResult(double n)
    {
        ++resultWrites;
        result = n;
    }
    bool Untouched() const
    {
        return reads == 0 && writes == 0 && positionReads == 0 && positionWrites == 0 && statReads == 0 && resultWrites == 0;
    }
    // Nothing written: no member, position or result. A stat row's own id
    // and answer are noted for statpin even with no lever on.
    bool Unwritten() const
    {
        return writes == 0 && positionWrites == 0 && resultWrites == 0;
    }
};

static Value Num(double v) { return Value{ false, v }; }

// An original that throws. The flag is volatile so the compiler cannot prove
// the code after the call unreachable (MSVC's C4702 under /W4 /WX).
static volatile bool g_throws = true;
static void GameThrows()
{
    if (g_throws) throw std::runtime_error("game threw");
}

static Lever SetLever(const std::string& args)
{
    return parsed(args).lever;
}

// `killgate`'s ops around one LoadDrops call: one 70-slot chances array, the game's
// LoadDrops and the re-entry as recorders, and the shared DropUniqueItems detour's call
// count. A type-45 call "rolls" a unique only while chances[45] is above the gate's die
// (about 100 outcomes, RDM section 13.1), so a vanilla 0 never reaches DropUniqueItems. Every
// read, write and call is logged with the chance the call saw.
struct FakeLoadDrops {
    std::vector<double> chances = std::vector<double>(70, 0.0);
    int type = 0;                 // the game's call's own drop type
    bool readable = true;
    bool writable = true;
    bool throwOnGame = false;
    bool throwOnReentry = false;
    std::function<void()> duringReentry;
    uint64_t uniques = 0;
    int reads = 0, writes = 0, gameCalls = 0, reentries = 0;
    std::vector<std::string> log;

    void Roll(int t)
    {
        if (t == 45 && chances[45] >= 100) ++uniques;
    }
    bool ReadChance(double& v)
    {
        ++reads;
        if (!readable) return false;
        v = chances[45];
        return true;
    }
    bool WriteChance(double v)
    {
        ++writes;
        if (!writable) return false;
        log.push_back("write " + NumberText(v));
        chances[45] = v;
        return true;
    }
    void CallGame()
    {
        ++gameCalls;
        log.push_back("game t=" + std::to_string(type) + " c45=" + NumberText(chances[45]));
        Roll(type);
        if (throwOnGame) throw std::runtime_error("game threw");
    }
    void CallReentry()
    {
        ++reentries;
        log.push_back("reenter t=45 c45=" + NumberText(chances[45]));
        Roll(45);
        if (duringReentry) duringReentry();
        if (throwOnReentry) throw std::runtime_error("re-entry threw");
    }
    uint64_t UniqueCalls() { return uniques; }
};

// `draws push`'s ops on the six lists, as the adapter's are on lootListUnique[0..5]: each
// list a vector of triples, every add and delete logged; `throwAfter` makes the n-th add
// throw, `unreadable` one list's length fail, `tailHidden` every tail check fail (a push that
// does not read back, NotHeld), and `meddle` lets the "game" change a list between the push
// and the removal.
struct FakeDrawLists {
    using T = ForgePact::Mark1Drops::Triple;
    std::array<std::vector<T>, 6> lists;
    int adds = 0, deletes = 0;
    int throwAfter = -1;
    int unreadable = -1;
    bool tailHidden = false;

    int Length(int k) { return k == unreadable ? -1 : (int)lists[(size_t)k].size(); }
    bool Add(int k, const T& t)
    {
        if (throwAfter >= 0 && adds >= throwAfter) return false;
        ++adds;
        lists[(size_t)k].push_back(t);
        return true;
    }
    bool TailHolds(int k, int before, const std::vector<ForgePact::Mark1Drops::PushEntry>& entries, const std::vector<int>& on, int& len)
    {
        const std::vector<T>& l = lists[(size_t)k];
        len = (int)l.size();
        if (tailHidden) return false;
        int at = before;
        for (size_t e = 0; e < entries.size() && e < on.size(); ++e)
            for (int n = 0; n < on[e]; ++n) {
                if (at >= len || !(l[(size_t)at] == entries[e].triple)) return false;
                ++at;
            }
        return at == len;
    }
    int DeleteLast(int k, int len, int n)
    {
        for (int i = 0; i < n; ++i) { lists[(size_t)k].pop_back(); ++deletes; }
        (void)len;
        return (int)lists[(size_t)k].size();
    }
};

// One DropItem call as the adapter drives the watch: the call begins, the tier is
// recorded, the draw reads its entry and picks it, the item is built, the call ends.
struct DrawCallItem {
    double t, sub, b, c;
    std::string name;
    double rarity = 6, tier = 5;
};

static DrawBuilt BuiltOf(const DrawCallItem& i)
{
    DrawBuilt d;
    d.t = i.t;
    d.b = i.b;
    d.c = i.c;
    d.rarity = i.rarity;
    d.tier = i.tier;
    d.name = i.name;
    return d;
}

// Read, pick and build `i` in the current scope; returns the built line.
static std::string DrawBuild(Draws& w, const DrawCallItem& i, const std::optional<DrawListCounts>& lists, int extraReads = 0)
{
    for (int r = 0; r < extraReads; ++r) w.NoteRead(true, i.t, i.sub + 1000 + r, i.b);
    w.NoteRead(true, i.t, i.sub, i.b);
    w.NotePick(true, i.sub, i.b);
    return w.NoteBuilt(BuiltOf(i), lists);
}

static DrawRoutes DetouredRoutes()
{
    DrawRoutes r;
    r.dropitem = "detoured";
    r.sattier = "detoured";
    r.retsat = "detoured";
    return r;
}

int main()
{
    std::vector<Row> allRows;
    for (int i = 0; i < kRowCount; ++i) allRows.push_back(static_cast<Row>(i));

    // ---- the rows ------------------------------------------------------------
    {
        bool names = RowName(Row::CheckTalentUse) == "CheckTalentUse" && RowName(Row::SkillsLeap) == "skillsLeap"
            && RowName(Row::PlayerJumpGravity) == "playerJumpGravity" && RowName(Row::StatJumpPower) == "StatJumpPower"
            && RowName(Row::PlaceMeeting) == "place_meeting" && RowName(Row::MoveAndCollide) == "move_and_collide";
        bool round = true;
        for (Row r : allRows) {
            Row back = Row::StatJumpPower;
            round = round && ParseRow(RowName(r), back) && back == r;
        }
        Row ci = Row::CheckTalentUse;
        const bool caseless = ParseRow("playermovement", ci) && ci == Row::PlayerMovement;
        // The nine scripts first, then the eleven builtins.
        const bool split = kScriptRowCount == 9 && kBuiltinRowCount == 11 && kRowCount == 20
            && !IsBuiltinRow(Row::StatJumpPower) && IsBuiltinRow(Row::PlaceMeeting) && IsBuiltinRow(Row::MoveAndCollide);
        check("rows/names_round_trip", names && round && caseless && split);
    }

    // ---- baseline: no lever --------------------------------------------------
    {
        Probe p;
        bool ok = true;
        std::string bad;
        for (Row r : allRows) {
            for (bool local : { true, false }) {
                FakeAccess a;
                a.members["player.moveSpeed"] = Num(4);
                a.result = 7;
                int called = 0;
                p.Around(r, 10, local, a, [&] { ++called; a.result = 42; });
                // Only the dispatcher's own id argument is read (statpin's
                // note); nothing is written anywhere.
                const bool reads = local && r == Row::ReturnSpecificStat
                    ? a.reads == 0 && a.positionReads == 0 && a.statReads == 1 : a.Untouched();
                if (called != 1 || a.result != 42 || !a.Unwritten() || !reads || a.members["player.moveSpeed"].number != 4) {
                    ok = false;
                    bad += std::string(RowName(r)) + (local ? "/local " : "/other ");
                }
            }
        }
        check("baseline/every_row_calls_the_original_once_and_writes_nothing", ok, bad);
        bool counted = true;
        for (Row r : allRows) counted = counted && p.Calls(r).local == 1 && p.Calls(r).all == 2;
        check("baseline/calls_are_counted_local_and_all", counted);
        check("baseline/nothing_applied", p.Applied().applied == 0 && p.Applied().moved == 0
              && p.Applied().changedDispatch == 0 && p.Applied().changedGetter == 0 && p.Applied().getterDirect == 0
              && p.LeverText() == "off");
    }
    {
        // A thrown original still propagates, with nothing written.
        Probe p;
        FakeAccess a;
        bool thrown = false;
        try { p.Around(Row::PlayerMovement, 1, true, a, [] { GameThrows(); }); }
        catch (const std::runtime_error&) { thrown = true; }
        check("baseline/a_throwing_original_propagates_untouched", thrown && a.Untouched());
        // Calls after the throw are not nested: depth went back to zero.
        Lever l = SetLever("set player.moveSpeed 9 during PlayerMovement");
        p.SetLever(l, 1);
        a.members["player.moveSpeed"] = Num(4);
        double seen = 0;
        p.Around(Row::PlayerMovement, 2, true, a, [&] { seen = a.members["player.moveSpeed"].number; });
        check("baseline/depth_recovers_after_a_throw", seen == 9);
    }
    {
        // A lever on a different row leaves every other row's call alone.
        Probe p;
        p.SetLever(SetLever("set player.moveSpeed 9 during CheckTalentUse"), 0);
        bool ok = true;
        for (Row r : allRows) {
            if (r == Row::CheckTalentUse) continue;
            FakeAccess a;
            a.members["player.moveSpeed"] = Num(4);
            int called = 0;
            p.Around(r, 1, true, a, [&] { ++called; });
            ok = ok && called == 1 && a.writes == 0 && a.reads == 0;
        }
        check("baseline/a_lever_leaves_other_rows_alone", ok);
    }

    // ---- set -----------------------------------------------------------------
    {
        Probe p;
        const std::string replaced = p.SetLever(SetLever("set profile.inputState[30] 1 during CheckTalentUse 30"), 100);
        FakeAccess a;
        a.members["profile.inputState[30]"] = Num(0);
        double during = -1;
        p.Around(Row::CheckTalentUse, 100, true, a, [&] { during = a.members["profile.inputState[30]"].number; });
        const bool ok = replaced.empty() && during == 1 && a.members["profile.inputState[30]"].number == 0
            && a.log.size() == 2 && a.log[0] == "write profile.inputState[30]=1" && a.log[1] == "write profile.inputState[30]=0"
            && p.Applied().applied == 1;
        check("lever/set_writes_before_the_original_and_restores_after", ok, a.log.empty() ? "" : a.log[0]);
    }
    {
        Probe p;
        p.SetLever(SetLever("set player.autoAim true during CheckTalentUse"), 0);
        FakeAccess a;
        a.members["player.autoAim"] = Value{ true, 0 };
        bool during = false;
        p.Around(Row::CheckTalentUse, 0, true, a, [&] { during = a.members["player.autoAim"].isBool && a.members["player.autoAim"].number == 1; });
        const Value after = a.members["player.autoAim"];
        check("lever/set_a_bool_and_restore_it", during && after.isBool && after.number == 0);
    }
    {
        Probe p;
        p.SetLever(SetLever("set profile.inputState[30] 1 during CheckTalentUse"), 0);
        FakeAccess a;
        a.members["profile.inputState[30]"] = Num(0);
        bool thrown = false;
        double during = -1;
        try {
            p.Around(Row::CheckTalentUse, 5, true, a, [&] {
                during = a.members["profile.inputState[30]"].number;
                GameThrows();
            });
        } catch (const std::runtime_error&) { thrown = true; }
        check("lever/set_restores_when_the_original_throws", thrown && during == 1
              && a.members["profile.inputState[30]"].number == 0 && a.writes == 2);
    }
    {
        Probe p;
        p.SetLever(SetLever("set profile.inputState[30] 1 during CheckTalentUse"), 0);
        FakeAccess a;
        a.members["profile.inputState[30]"] = Num(0);
        double during = -1;
        p.Around(Row::CheckTalentUse, 5, false, a, [&] { during = a.members["profile.inputState[30]"].number; });
        check("lever/set_only_for_the_local_player", during == 0 && a.Untouched() && p.Applied().applied == 0);
    }
    {
        Probe p;
        p.SetLever(SetLever("set player.moveSpeed 9 during PlayerMovement 10"), 100);
        FakeAccess a;
        a.members["player.moveSpeed"] = Num(4);
        double last = 0, past = 0, before = 0;
        p.Around(Row::PlayerMovement, 99, true, a, [&] { before = a.members["player.moveSpeed"].number; });
        p.Around(Row::PlayerMovement, 109, true, a, [&] { last = a.members["player.moveSpeed"].number; });
        p.Around(Row::PlayerMovement, 110, true, a, [&] { past = a.members["player.moveSpeed"].number; });
        const bool budget = before == 4 && last == 9 && past == 4 && p.Applied().applied == 1;
        check("lever/set_only_within_its_frame_budget", budget);
        const std::vector<std::string> early = p.Tick(109);
        const std::vector<std::string> ended = p.Tick(110);
        check("lever/the_budget_ends_with_a_line", early.empty() && ended.size() == 1
              && ended[0] == "mark1probe: lever set(player.moveSpeed=9,during=PlayerMovement,frames=10) ended applied=1"
              && p.LeverText() == "off", ended.empty() ? "" : ended[0]);
    }
    {
        Probe p;
        p.SetLever(SetLever("set player.missing 1 during PlayerMovement"), 0);
        FakeAccess a;
        int called = 0;
        p.Around(Row::PlayerMovement, 0, true, a, [&] { ++called; });
        check("lever/set_an_unreadable_member_writes_nothing", called == 1 && a.writes == 0 && p.Applied().missing == 1
              && p.Applied().applied == 0);
    }
    {
        // A row the game calls inside itself is written once, by the outer call.
        Probe p;
        p.SetLever(SetLever("scale player.moveSpeed 2 during PlayerMovement"), 0);
        FakeAccess a;
        a.members["player.moveSpeed"] = Num(4);
        double inner = 0;
        p.Around(Row::PlayerMovement, 0, true, a, [&] {
            p.Around(Row::PlayerMovement, 0, true, a, [&] { inner = a.members["player.moveSpeed"].number; });
        });
        check("lever/a_nested_call_of_the_row_applies_once", inner == 8 && a.members["player.moveSpeed"].number == 4
              && a.writes == 2 && p.Applied().applied == 1);
    }

    // ---- scale ---------------------------------------------------------------
    {
        Probe p;
        p.SetLever(SetLever("scale player.moveSpeed 0.5 during PlayerMovement"), 0);
        FakeAccess a;
        a.members["player.moveSpeed"] = Num(6);
        double during = 0;
        p.Around(Row::PlayerMovement, 0, true, a, [&] { during = a.members["player.moveSpeed"].number; });
        check("lever/scale_multiplies_before_and_restores_after", during == 3 && a.members["player.moveSpeed"].number == 6
              && p.Applied().applied == 1);
        bool thrown = false;
        try { p.Around(Row::PlayerMovement, 1, true, a, [] { GameThrows(); }); } catch (const std::runtime_error&) { thrown = true; }
        check("lever/scale_restores_when_the_original_throws", thrown && a.members["player.moveSpeed"].number == 6);
        FakeAccess other;
        other.members["player.moveSpeed"] = Num(6);
        p.Around(Row::PlayerMovement, 2, false, other, [] {});
        check("lever/scale_only_for_the_local_player", other.Untouched());
    }
    {
        Probe p;
        p.SetLever(SetLever("scale player.autoAim 2 during CheckTalentUse"), 0);
        FakeAccess a;
        a.members["player.autoAim"] = Value{ true, 1 };
        p.Around(Row::CheckTalentUse, 0, true, a, [] {});
        check("lever/scale_leaves_a_bool_alone", a.writes == 0 && p.Applied().missing == 1);
    }

    // ---- move ----------------------------------------------------------------
    {
        Probe p;
        p.SetLever(SetLever("move PlayerMovement 0.5"), 0);
        FakeAccess a;
        a.x = 100;
        a.y = 200;
        p.Around(Row::PlayerMovement, 0, true, a, [&] { a.x += 8; a.y -= 4; });
        check("lever/move_halves_the_calls_displacement", a.x == 104 && a.y == 198 && p.Applied().moved == 1);
        p.Around(Row::PlayerMovement, 1, true, a, [] {});
        check("lever/move_leaves_a_call_that_did_not_move_alone", a.x == 104 && a.y == 198 && a.positionWrites == 1
              && p.Applied().still == 1);
        FakeAccess other;
        p.Around(Row::PlayerMovement, 2, false, other, [&] { other.x += 8; });
        check("lever/move_only_for_the_local_player", other.x == 108 && other.positionWrites == 0 && other.positionReads == 0);
        FakeAccess wrongRow;
        p.Around(Row::TalentUse, 3, true, wrongRow, [&] { wrongRow.x += 8; });
        check("lever/move_only_on_its_row", wrongRow.x == 108 && wrongRow.positionWrites == 0);
    }
    {
        Probe p;
        p.SetLever(SetLever("move skillsLeap 3"), 0);
        FakeAccess a;
        a.x = 10;
        a.y = 10;
        p.Around(Row::SkillsLeap, 0, true, a, [&] { a.x += 20; });
        check("lever/move_triples_the_calls_displacement", a.x == 70 && a.y == 10 && p.Applied().moved == 1);
    }

    // ---- stat ----------------------------------------------------------------
    {
        Probe p;
        p.SetLever(SetLever("stat 64 0.5"), 0);
        FakeAccess a;
        a.statId = 64;
        p.Around(Row::ReturnSpecificStat, 0, true, a, [&] { a.result = 300; });
        check("lever/stat_scales_the_matching_ids_number", a.result == 150 && p.Applied().changedDispatch == 1
              && p.Applied().changedGetter == 0);
        FakeAccess otherId;
        otherId.statId = 66;
        p.Around(Row::ReturnSpecificStat, 0, true, otherId, [&] { otherId.result = 300; });
        check("lever/stat_leaves_another_id_alone", otherId.result == 300 && otherId.resultWrites == 0);
        FakeAccess otherSelf;
        p.Around(Row::ReturnSpecificStat, 0, false, otherSelf, [&] { otherSelf.result = 300; });
        check("lever/stat_only_for_the_local_player", otherSelf.result == 300 && otherSelf.Untouched());
        FakeAccess text;
        text.resultIsNumber = false;
        p.Around(Row::ReturnSpecificStat, 0, true, text, [] {});
        check("lever/stat_counts_a_non_number_as_skip", text.resultWrites == 0 && p.Applied().skip == 1);
        FakeAccess noId;
        noId.hasStatId = false;
        p.Around(Row::ReturnSpecificStat, 0, true, noId, [&] { noId.result = 300; });
        check("lever/stat_leaves_a_call_without_an_id_alone", noId.result == 300 && noId.resultWrites == 0);
    }
    {
        // Compiled GML calls StatMovementSpeed directly as well as through the
        // dispatcher, so `stat 64` scales the getter row's own number too. The
        // getter takes no stat-id argument: the row itself is the match.
        Probe p;
        p.SetLever(SetLever("stat 64 0.5"), 0);
        FakeAccess a;
        p.Around(Row::StatMovementSpeed, 0, true, a, [&] { a.result = 300; });
        check("lever/stat_scales_the_getter_row_too", a.result == 150 && a.resultWrites == 1 && a.statReads == 0
              && p.Applied().changedGetter == 1 && p.Applied().getterDirect == 1 && p.Applied().changedDispatch == 0,
              NumberText(a.result));
        // Negative controls: another id's getter, and the getter for another self.
        FakeAccess jump;
        p.Around(Row::StatJumpPower, 1, true, jump, [&] { jump.result = 300; });
        FakeAccess otherSelf;
        p.Around(Row::StatMovementSpeed, 2, false, otherSelf, [&] { otherSelf.result = 300; });
        check("lever/stat_getter_only_its_own_id_and_the_local_player", jump.result == 300 && jump.Untouched()
              && otherSelf.result == 300 && otherSelf.Untouched() && p.Applied().changedGetter == 1);
    }
    {
        // The dispatcher handing id 64 to the detoured getter: the getter
        // scales, and the dispatcher returns that number as it is. Scaled
        // twice, a 0.5 lever would read as 0.25 (75, not 150).
        Probe p;
        p.SetLever(SetLever("stat 64 0.5"), 0);
        FakeAccess a;
        a.statId = 64;
        p.Around(Row::ReturnSpecificStat, 0, true, a, [&] {
            p.Around(Row::StatMovementSpeed, 0, true, a, [&] { a.result = 300; });
        });
        check("lever/stat_scales_once_through_the_dispatcher", a.result == 150 && a.resultWrites == 1
              && p.Applied().changedGetter == 1 && p.Applied().changedDispatch == 0 && p.Applied().getterDirect == 0,
              NumberText(a.result));
        // The next dispatcher call is a new call: it scales once again.
        p.Around(Row::ReturnSpecificStat, 1, true, a, [&] {
            p.Around(Row::StatMovementSpeed, 1, true, a, [&] { a.result = 300; });
        });
        check("lever/stat_scales_each_dispatcher_call_once", a.result == 150 && a.resultWrites == 2
              && p.Applied().changedGetter == 2 && p.Applied().changedDispatch == 0);
        const std::vector<std::string> ended = p.Tick(600);
        check("lever/stat_end_line_counts_each_route", ended.size() == 1
              && ended[0] == "mark1probe: lever stat(64*0.5,frames=600) ended changed-dispatch=0 changed-getter=2 getter-direct=0 skip=0",
              ended.empty() ? "" : ended[0]);
    }
    {
        // An id with no getter row among the rows (72, attack speed): the
        // dispatcher alone scales it, and a getter row's call is untouched.
        Probe p;
        p.SetLever(SetLever("stat 72 0.5"), 0);
        FakeAccess a;
        a.statId = 72;
        p.Around(Row::ReturnSpecificStat, 0, true, a, [&] { a.result = 300; });
        check("lever/stat_without_a_getter_row_scales_the_dispatcher_alone", a.result == 150
              && p.Applied().changedDispatch == 1 && p.Applied().changedGetter == 0 && p.Applied().getterDirect == 0,
              NumberText(a.result));
        // For such an id, stat acts on ReturnSpecificStat only.
        FakeAccess wrongRow;
        p.Around(Row::StatMovementSpeed, 1, true, wrongRow, [&] { wrongRow.result = 300; });
        check("lever/stat_only_on_ReturnSpecificStat", wrongRow.result == 300 && wrongRow.Untouched()
              && p.Applied().changedGetter == 0);
    }

    // ---- one lever at a time -------------------------------------------------
    {
        Probe p;
        const std::string first = p.SetLever(SetLever("set player.moveSpeed 9 during PlayerMovement"), 0);
        const std::string second = p.SetLever(SetLever("stat 64 2"), 0);
        FakeAccess a;
        a.members["player.moveSpeed"] = Num(4);
        double during = 0;
        p.Around(Row::PlayerMovement, 1, true, a, [&] { during = a.members["player.moveSpeed"].number; });
        check("lever/a_second_lever_replaces_the_first", first.empty()
              && second == "set(player.moveSpeed=9,during=PlayerMovement,frames=600)" && during == 4 && a.writes == 0
              && p.LeverText() == "stat(64*2,frames=600)", second);
        p.Off();
        FakeAccess b;
        p.Around(Row::ReturnSpecificStat, 2, true, b, [&] { b.result = 5; });
        check("lever/off_turns_every_lever_off", p.LeverText() == "off" && b.result == 5 && b.Unwritten());
    }
    {
        const bool texts = LeverText(SetLever("scale pst.spd 1.5 during TalentUse 90")) == "scale(pst.spd*1.5,during=TalentUse,frames=90)"
            && LeverText(SetLever("move PlayerMovement 3 1200")) == "move(PlayerMovement*3,frames=1200)"
            && LeverText(SetLever("set input[12] 1 during CheckTalentUse")) == "set(input[12]=1,during=CheckTalentUse,frames=600)"
            && LeverText(SetLever("hold player.hSpeedFactor x0.5 during PlayerMovement 240"))
                == "hold(player.hSpeedFactor*0.5,during=PlayerMovement,frames=240)"
            && LeverText(SetLever("hold effect[182] 0 during PlayerMovement")) == "hold(effect[182]=0,during=PlayerMovement,frames=600)";
        check("lever/text_has_no_spaces", texts);
        const Lever l = SetLever("set profile.inputState[30] 1 during CheckTalentUse");
        const std::string a = LeverReply(l, 100, "playerProfileSlot", "0", "");
        const std::string b = LeverReply(SetLever("move PlayerMovement 0.5"), 7, "", "", "stat(64*2,frames=600)");
        check("lever/reply_names_scope_value_and_restored_value",
              a == "mark1probe set: profile.inputState[30] := 1 during CheckTalentUse for 600 frames from f=100 scope=playerProfileSlot restores=0"
              && b == "mark1probe move: PlayerMovement displacement x0.5 for 600 frames from f=7 (replaces stat(64*2,frames=600))", a + " | " + b);
        // Every lever's reply names the frame it starts on.
        bool every = true;
        std::string bad;
        for (const char* form : { "set player.x 1 during TalentUse", "scale player.x 2 during TalentUse", "move skillsLeap 3",
                                  "stat 64 0.5", "hold player.x 1 during TalentUse", "statpin 64 150" }) {
            const std::string reply = LeverReply(SetLever(form), 4321, "player", "1", "");
            if (reply.find(" from f=4321") == std::string::npos) { every = false; bad += reply + " | "; }
        }
        check("lever/every_reply_names_its_start_frame", every, bad);
    }

    // ---- show, casts, hook, status --------------------------------------------
    {
        Probe p;
        p.NoteHook(Row::CheckTalentUse, HookState::Detoured);
        p.NoteHook(Row::TalentUse, HookState::Detoured);
        p.NoteHook(Row::PlayerMovement, HookState::Held, "co-op movement hook");
        p.NoteHook(Row::SkillsLeap, HookState::Failed);
        check("hook/line_and_held_rows", p.HookLine() == "mark1probe hook: 2 detoured, 1 failed, 1 held, 0 not-found"
              && p.HeldLines().size() == 1 && p.HeldLines()[0] == "  PlayerMovement held by co-op movement hook"
              && p.NotFoundLines().empty(), p.HookLine());
        const std::vector<std::string> status = p.StatusLines();
        check("status/first_lines", status.size() > 3 && status[0] == "mark1probe: research build"
              && status[1] == "mark1probe: rows=20 hooked=2 held=1 lever=off watch=off statlog=off", status.size() > 1 ? status[1] : "");
        p.NoteCast(12);
        p.NoteCast(12);
        p.NoteCast(3);
        check("show/casts_per_talent_id", p.CastsText() == "casts=3:1,12:2", p.CastsText());
        const std::vector<std::string> show = p.ShowLines();
        bool rowLine = false;
        for (const std::string& l : show) rowLine = rowLine || l == "  PlayerMovement calls=0/0 held";
        check("show/lines", show.size() == static_cast<size_t>(kRowCount + 2)
              && show[0] == "mark1probe show: lever=off applied=0 missing=0 moved=0 still=0 changed-dispatch=0 changed-getter=0 getter-direct=0 skip=0"
              && rowLine && show.back() == "casts=3:1,12:2", show.empty() ? "" : show[0]);
        p.ResetCounts();
        check("show/reset_clears_counts_and_casts", p.CastsText() == "casts=none");
    }
    {
        check("args/line", ArgsLine(1, 120, 3, { "0", "2", "-4" }) == "ctu #1 f=120 argc=3 a0=0 a1=2 a2=-4"
              && ArgsLine(2, 121, 1, { "5" }) == "ctu #2 f=121 argc=1 a0=5 a1=none a2=none");
        Probe p;
        p.StartArgs(2);
        const int a = p.TakeArg(), b = p.TakeArg(), c = p.TakeArg();
        check("args/takes_n_then_stops", a == 1 && b == 2 && c == 0);
    }

    // ---- watch, count, pos ----------------------------------------------------
    {
        Probe p;
        p.StartWatch(Scope::Pst, 5, 10, false, Row::CheckTalentUse);
        const auto first = p.ObserveWatch(10, { { "jumping", "0" }, { "z", "0" } });
        const auto second = p.ObserveWatch(11, { { "jumping", "1" }, { "z", "0" }, { "new", "3" } });
        const auto ended = p.Tick(15);
        check("watch/changes_only_from_the_second_snapshot", first.empty() && second.size() == 2
              && second[0] == "watch f=11 pst.jumping=0->1" && second[1] == "watch f=11 pst.new=none->3"
              && ended.size() == 1 && ended[0] == "watch end scope=pst frames=5 lines=2 dropped=0" && !p.WatchOn(),
              second.empty() ? "" : second[0]);
    }
    {
        Probe p;
        p.StartWatch(Scope::Input, 100, 0, true, Row::TalentUse);
        FakeAccess a;
        const bool pending = p.WatchOn() && !p.WatchRunning(3);
        p.Around(Row::TalentUse, 4, false, a, [] {});
        const bool stillPending = !p.WatchRunning(4);
        p.Around(Row::TalentUse, 5, true, a, [] {});
        p.ObserveWatch(5, { { "30", "0" } });
        const auto lines = p.ObserveWatch(6, { { "30", "1" } });
        check("watch/after_a_row_starts_at_its_next_local_call", pending && stillPending && p.WatchRunning(5)
              && lines.size() == 1 && lines[0] == "watch f=6 input[30]=0->1", lines.empty() ? "" : lines[0]);
    }
    {
        // The cap is reached over several snapshots; the one that crosses it
        // prints what still fits, then the end line with the rest dropped.
        Probe p;
        p.StartWatch(Scope::Player, 3600, 0, false, Row::CheckTalentUse);
        std::map<std::string, std::string> snap;
        for (int k = 0; k < kWatchMaxLines; ++k) snap["m" + std::to_string(k)] = "0";
        p.ObserveWatch(0, snap);
        size_t lines = 0;
        for (int k = 0; k < kWatchMaxLines - 5; ++k) snap["m" + std::to_string(k)] = "1";
        lines += p.ObserveWatch(1, snap).size();
        for (int k = 0; k < 15; ++k) snap["m" + std::to_string(k)] = "2";
        const auto last = p.ObserveWatch(2, snap);
        const bool ended = !p.WatchOn() && p.ObserveWatch(3, snap).empty() && p.Tick(4).empty();
        check("watch/a_line_cap_ends_it_with_a_dropped_count", lines == static_cast<size_t>(kWatchMaxLines - 5)
              && last.size() == 6 && last[5] == "watch end scope=player frames=2 lines=" + std::to_string(kWatchMaxLines)
                 + " dropped=10 (line cap)" && ended, last.empty() ? "" : last.back());
    }
    {
        Probe p;
        p.StartCount("Wall_Parent_obj", 10, 0);
        const std::string a = p.ObserveCount(0, 3), b = p.ObserveCount(1, 3), c = p.ObserveCount(2, 5);
        const auto end = p.Tick(10);
        check("count/one_line_per_change", a == "count f=0 Wall_Parent_obj=none->3" && b.empty()
              && c == "count f=2 Wall_Parent_obj=3->5" && end.size() == 1
              && end[0] == "count end Wall_Parent_obj frames=10 changes=2", c);
    }
    {
        Probe p;
        check("pos/start_line", PosStartLine(true, 512, 300.5) == "pos cursor x=512 y=300.5"
              && PosStartLine(false, 0, 0) == "pos cursor=unreadable");
        p.StartPos(20, 0);
        FakeAccess a;
        p.Around(Row::PlayerMovement, 1, true, a, [&] { a.x += 3; a.y += 4; });
        p.Around(Row::PlayerMovement, 2, true, a, [] {});
        p.Around(Row::TalentUse, 3, true, a, [&] {
            p.Around(Row::PlayerMovement, 3, true, a, [&] { a.x += 6; a.y += 8; });
        });
        const auto lines = p.TakeLines();
        const auto end = p.Tick(20);
        // No frame sample was taken: in-rows counts the outermost calls only
        // (5 + 10), and total and outside measured nothing, so they say none.
        check("pos/lines_per_moving_call_and_a_total", lines.size() == 3 && lines[0] == "pos f=1 PlayerMovement dx=3 dy=4"
              && lines[1] == "pos f=3 PlayerMovement dx=6 dy=8" && lines[2] == "pos f=3 TalentUse dx=6 dy=8"
              && end.size() == 1
              && end[0] == "pos total=none in-rows=15 outside=none mode=none ms=none frames=20 samples=0 unread=0 movers=TalentUse:1,PlayerMovement:2",
              end.empty() ? (lines.empty() ? "" : lines[0]) : end[0]);
    }
    {
        // The frame samples: a step the rows account for prints nothing; a
        // step no row made prints an `outside` line with its residual; a step
        // a row made only part of prints the rest; an unreadable sample is
        // counted, never taken as a position.
        Probe p;
        p.StartPos(10, 0);
        FakeAccess a;
        a.x = 100;
        a.y = 200;
        p.ObservePosFrame(0, true, a.x, a.y);
        p.Around(Row::PlayerMovement, 1, true, a, [&] { a.x += 3; a.y += 4; });
        p.ObservePosFrame(1, true, a.x, a.y);               // only a row moved: outside 0
        const auto rowOnly = p.TakeLines();
        a.x += 6;
        a.y += 8;
        p.ObservePosFrame(2, true, a.x, a.y);               // moved with no row call
        const auto noRow = p.TakeLines();
        p.ObservePosFrame(3, false, 0, 0);                  // unreadable
        p.ObservePosFrame(4, true, std::nan(""), a.y);      // not a position either
        p.Around(Row::PlayerMovement, 5, true, a, [&] { a.x += 3; a.y += 4; });
        p.ObservePosFrame(5, true, a.x + 3, a.y + 4);       // a row made half the step
        const auto partly = p.TakeLines();
        const auto end = p.Tick(10);
        check("pos/a_step_the_rows_made_prints_no_outside_line", rowOnly.size() == 1
              && rowOnly[0] == "pos f=1 PlayerMovement dx=3 dy=4", rowOnly.empty() ? "" : rowOnly.back());
        check("pos/a_step_no_row_made_prints_an_outside_line", noRow.size() == 1 && noRow[0] == "pos f=2 outside dx=6 dy=8",
              noRow.empty() ? "" : noRow[0]);
        check("pos/a_step_a_row_made_part_of_prints_the_rest", partly.size() == 2
              && partly[0] == "pos f=5 PlayerMovement dx=3 dy=4" && partly[1] == "pos f=5 outside dx=3 dy=4",
              partly.size() < 2 ? "" : partly[1]);
        check("pos/total_outside_samples_and_unread", end.size() == 1
              && end[0] == "pos total=25 in-rows=10 outside=15 mode=10 ms=none frames=10 samples=4 unread=2 movers=PlayerMovement:2,outside:2",
              end.empty() ? "" : end[0]);
    }
    {
        // One readable sample measures no step: total and outside say none,
        // never a 0 that reads as standing still.
        Probe p;
        p.StartPos(5, 0);
        p.ObservePosFrame(0, true, 10, 10);
        p.ObservePosFrame(1, false, 0, 0);
        p.ObservePosFrame(7, true, 50, 50);                 // past the run: ignored
        const auto end = p.Tick(5);
        check("pos/fewer_than_two_samples_print_none", end.size() == 1
              && end[0] == "pos total=none in-rows=0 outside=none mode=none ms=none frames=5 samples=1 unread=1 movers=none",
              end.empty() ? "" : end[0]);
        // A standing player with two samples does measure: total=0, outside=0.
        Probe q;
        q.StartPos(5, 0);
        q.ObservePosFrame(0, true, 10, 10);
        q.ObservePosFrame(1, true, 10, 10);
        const auto still = q.Tick(5);
        check("pos/two_still_samples_measure_zero", still.size() == 1 && q.TakeLines().empty()
              && still[0] == "pos total=0 in-rows=0 outside=0 mode=none ms=none frames=5 samples=2 unread=0 movers=none",
              still.empty() ? "" : still[0]);
    }
    {
        // With no row hooked at all, the frame samples alone measure the walk:
        // the modal step is the steady one (the start-up steps are rarer), a
        // standing frame is not a step, and ms is the mean time between samples.
        Probe p;
        p.StartPos(10, 0);
        const double xs[] = { 100, 101.5, 104, 107.25, 110.5, 113.75, 117, 117 };
        const double clock[] = { 1000, 1013, 1027, 1040, 1054, 1067, 1081, 1094 };
        for (int k = 0; k < 8; ++k) p.ObservePosFrame(k, true, xs[k], 50, clock[k]);
        const auto end = p.Tick(10);
        check("pos/samples_with_no_row_hooked_give_mode_and_ms", end.size() == 1 && p.TakeLines().size() == 6
              && end[0] == "pos total=17 in-rows=0 outside=17 mode=3.25 ms=13.43 frames=10 samples=8 unread=0 movers=outside:6",
              end.empty() ? "" : end[0]);
        // A clock read on a frame whose position was not is still a frame.
        Probe q;
        q.StartPos(5, 0);
        q.ObservePosFrame(0, true, 0, 0, 0);
        q.ObservePosFrame(1, false, 0, 0, 20);
        q.ObservePosFrame(2, true, 0, 4, 40);
        const auto qe = q.Tick(5);
        check("pos/ms_counts_every_frame_and_mode_a_vertical_step", qe.size() == 1
              && qe[0] == "pos total=4 in-rows=0 outside=4 mode=4 ms=20 frames=5 samples=2 unread=1 movers=outside:1",
              qe.empty() ? "" : qe[0]);
    }

    // ---- the builtin rows (phase 2) --------------------------------------------
    {
        // A builtin row is a row: with no lever it passes through and counts;
        // `move` scales its displacement; `pos` names it as a mover, inside a
        // script row or on its own.
        Probe p;
        FakeAccess a;
        int called = 0;
        p.Around(Row::PlaceMeeting, 1, true, a, [&] { ++called; });
        p.Around(Row::PlaceMeeting, 1, false, a, [&] { ++called; });
        check("builtin/a_builtin_row_passes_through_and_counts", called == 2 && a.Untouched()
              && p.Calls(Row::PlaceMeeting).local == 1 && p.Calls(Row::PlaceMeeting).all == 2);
        p.SetLever(SetLever("move mp_linear_step 0.5 240"), 0);
        FakeAccess m;
        m.x = 10;
        p.Around(Row::PlayerMovement, 2, true, m, [&] {
            p.Around(Row::MpLinearStep, 2, true, m, [&] { m.x += 6; });
        });
        check("builtin/move_scales_a_builtin_rows_displacement", m.x == 13 && p.Applied().moved == 1 && m.positionWrites == 1,
              NumberText(m.x));
        Probe q;
        q.StartPos(10, 0);
        FakeAccess w;
        q.Around(Row::PlayerMovement, 1, true, w, [&] {
            q.Around(Row::MpLinearStep, 1, true, w, [&] { w.x += 4; });
        });
        const auto lines = q.TakeLines();
        const auto end = q.Tick(10);
        check("builtin/pos_names_a_builtin_mover", lines.size() == 2 && lines[0] == "pos f=1 mp_linear_step dx=4 dy=0"
              && lines[1] == "pos f=1 PlayerMovement dx=4 dy=0" && end.size() == 1
              && end[0].find("movers=PlayerMovement:1,mp_linear_step:1") != std::string::npos, end.empty() ? "" : end[0]);
    }
    {
        // A builtin the runtime does not know is listed, never failed.
        Probe p;
        for (Row r : allRows) p.NoteHook(r, HookState::Detoured);
        p.NoteHook(Row::MoveAndCollide, HookState::NotFound);
        p.NoteHook(Row::MpPotentialStepObject, HookState::NotFound);
        const auto nf = p.NotFoundLines();
        bool showLine = false;
        for (const std::string& l : p.ShowLines()) showLine = showLine || l == "  move_and_collide calls=0/0 not-found";
        check("hook/a_builtin_not_found_is_listed_not_failed", p.HookLine() == "mark1probe hook: 18 detoured, 0 failed, 0 held, 2 not-found"
              && nf.size() == 1 && nf[0] == "  not-found: mp_potential_step_object, move_and_collide (no builtin by that name on this runtime;"
                 " not counted as failed)" && showLine, p.HookLine());
    }

    // ---- statlog ----------------------------------------------------------------
    {
        Probe p;
        p.StartStatlog(64, 10, 100);
        FakeAccess a;
        a.statId = 64;
        // A dispatcher call inside PlayerMovement inside CheckTalentUse.
        p.Around(Row::CheckTalentUse, 100, true, a, [&] {
            p.Around(Row::PlayerMovement, 100, true, a, [&] {
                p.Around(Row::ReturnSpecificStat, 100, true, a, [&] { a.result = 300; });
            });
        });
        // On its own, a different number.
        p.Around(Row::ReturnSpecificStat, 101, true, a, [&] { a.result = 150; });
        // Another id, and another self: nothing.
        FakeAccess other;
        other.statId = 66;
        p.Around(Row::ReturnSpecificStat, 102, true, other, [&] { other.result = 9; });
        FakeAccess notLocal;
        p.Around(Row::ReturnSpecificStat, 102, false, notLocal, [&] { notLocal.result = 9; });
        // The dispatcher handing the id to its getter: one line, the dispatcher's.
        p.Around(Row::ReturnSpecificStat, 103, true, a, [&] {
            p.Around(Row::StatMovementSpeed, 103, true, a, [&] { a.result = 280; });
        });
        // The getter called directly: its own line.
        FakeAccess direct;
        p.Around(Row::PlayerMovement, 104, true, direct, [&] {
            p.Around(Row::StatMovementSpeed, 104, true, direct, [&] { direct.result = 290; });
        });
        // A non-number answer prints its kind.
        FakeAccess text;
        text.statId = 64;
        text.resultIsNumber = false;
        text.resultKind = "ref";
        p.Around(Row::ReturnSpecificStat, 105, true, text, [] {});
        const auto lines = p.TakeLines();
        const auto end = p.Tick(110);
        const bool ok = lines.size() == 5
            && lines[0] == "statlog f=100 id=64 ReturnSpecificStat=300 in=PlayerMovement,CheckTalentUse"
            && lines[1] == "statlog f=101 id=64 ReturnSpecificStat=150 in=none"
            && lines[2] == "statlog f=103 id=64 ReturnSpecificStat=280 in=none"
            && lines[3] == "statlog f=104 id=64 StatMovementSpeed=290 in=PlayerMovement"
            && lines[4] == "statlog f=105 id=64 ReturnSpecificStat=ref in=none";
        check("statlog/one_line_per_local_answer_with_the_rows_around_it", ok, lines.empty() ? "" : lines.back());
        check("statlog/end_line_counts_min_and_max", end.size() == 1
              && end[0] == "statlog end id=64 frames=10 calls=4 direct=1 min=150 max=300 lines=5 dropped=0",
              end.empty() ? "" : end[0]);
        // The baseline: statlog reads, never writes, and every answer is the original's.
        check("statlog/writes_nothing", a.Unwritten() && direct.Unwritten() && text.Unwritten() && a.result == 280
              && direct.result == 290 && other.Unwritten() && notLocal.Untouched());
    }
    {
        // Off and out of frames: no line.
        Probe p;
        FakeAccess a;
        a.statId = 64;
        p.Around(Row::ReturnSpecificStat, 1, true, a, [&] { a.result = 300; });
        p.StartStatlog(64, 5, 10);
        p.Around(Row::ReturnSpecificStat, 15, true, a, [&] { a.result = 300; });
        const bool none = p.TakeLines().empty();
        const auto end = p.Tick(15);
        p.StartStatlog(64, 5, 20);
        p.Off();
        p.Around(Row::ReturnSpecificStat, 21, true, a, [&] { a.result = 300; });
        check("statlog/no_line_before_after_or_once_off", none && p.TakeLines().empty() && p.Tick(30).empty()
              && end.size() == 1 && end[0] == "statlog end id=64 frames=5 calls=0 direct=0 min=none max=none lines=0 dropped=0",
              end.empty() ? "" : end[0]);
    }
    {
        // Past the cap the lines are counted, not printed.
        Probe p;
        p.StartStatlog(64, 3600, 0);
        FakeAccess a;
        a.statId = 64;
        for (int k = 0; k < kStatlogMaxLines + 7; ++k) p.Around(Row::ReturnSpecificStat, 1, true, a, [&] { a.result = 1; });
        const size_t printed = p.TakeLines().size();
        const auto end = p.Tick(3600);
        check("statlog/a_line_cap_counts_the_rest", printed == static_cast<size_t>(kStatlogMaxLines) && end.size() == 1
              && end[0].find("lines=" + std::to_string(kStatlogMaxLines) + " dropped=7") != std::string::npos,
              end.empty() ? "" : end[0]);
    }

    // ---- statpin ----------------------------------------------------------------
    {
        Probe p;
        double pin = 0;
        int64_t at = 0;
        const bool nothingYet = !p.LastStat(64, pin, at);
        FakeAccess a;
        a.statId = 64;
        p.Around(Row::ReturnSpecificStat, 10, true, a, [&] { a.result = 300; });
        p.Around(Row::ReturnSpecificStat, 11, true, a, [&] { a.result = 310; });
        FakeAccess other;   // another self's answer is never a pin
        other.statId = 64;
        p.Around(Row::ReturnSpecificStat, 12, false, other, [&] { other.result = 999; });
        const bool seen = p.LastStat(64, pin, at) && pin == 310 && at == 11;
        Lever l = SetLever("statpin 64 150");
        l.value = Value{ false, pin };
        p.SetLever(l, 20);
        // The cast's dip: the original answers 150, the pin answers 310.
        p.Around(Row::ReturnSpecificStat, 20, true, a, [&] { a.result = 150; });
        const double dispatched = a.result;
        FakeAccess getter;
        p.Around(Row::StatMovementSpeed, 21, true, getter, [&] { getter.result = 150; });
        FakeAccess nested;
        nested.statId = 64;
        p.Around(Row::ReturnSpecificStat, 22, true, nested, [&] {
            p.Around(Row::StatMovementSpeed, 22, true, nested, [&] { nested.result = 150; });
        });
        FakeAccess otherId;
        otherId.statId = 72;
        p.Around(Row::ReturnSpecificStat, 23, true, otherId, [&] { otherId.result = 5; });
        FakeAccess notLocal;
        notLocal.statId = 64;
        p.Around(Row::ReturnSpecificStat, 24, false, notLocal, [&] { notLocal.result = 150; });
        check("statpin/answers_with_the_last_answer_before_it", nothingYet && seen && dispatched == 310 && getter.result == 310
              && nested.result == 310 && nested.resultWrites == 1 && otherId.result == 5 && otherId.Unwritten()
              && notLocal.result == 150 && notLocal.Untouched()
              && p.Applied().changedDispatch == 1 && p.Applied().changedGetter == 2 && p.Applied().getterDirect == 1,
              NumberText(dispatched));
        const std::string reply = LeverReply(l, 20, "", "the last local answer, f=11", "");
        check("statpin/reply_names_the_pinned_value", reply
              == "mark1probe statpin: id 64 pinned=310 (the last local answer, f=11) on ReturnSpecificStat and StatMovementSpeed"
                 " for 150 frames from f=20" && p.LeverText() == "statpin(64=310,frames=150)", reply);
        const auto end = p.Tick(170);
        check("statpin/ends_with_its_counts", end.size() == 1
              && end[0] == "mark1probe: lever statpin(64=310,frames=150) ended changed-dispatch=1 changed-getter=2 getter-direct=1 skip=0",
              end.empty() ? "" : end[0]);
    }

    // ---- hold -------------------------------------------------------------------
    {
        // Written before each call, not restored after it; restored once,
        // when the lever ends.
        Probe p;
        FakeAccess a;
        a.members["player.hSpeedFactor"] = Num(1);
        p.StartHold(SetLever("hold player.hSpeedFactor x0.5 during PlayerMovement 3"), 10, true, Num(1));
        double during = 0;
        p.Around(Row::PlayerMovement, 10, true, a, [&] { during = a.members["player.hSpeedFactor"].number; });
        const double between = a.members["player.hSpeedFactor"].number;
        p.Around(Row::PlayerMovement, 12, true, a, [] {});
        const std::string early = p.RestoreHold(a, 12, false);
        const auto ended = p.Tick(13);
        const std::string restored = p.RestoreHold(a, 13, false);
        const std::string twice = p.RestoreHold(a, 14, true);
        check("hold/writes_before_each_call_and_restores_once_at_the_end", during == 0.5 && between == 0.5 && early.empty()
              && ended.size() == 1 && ended[0] == "mark1probe: lever hold(player.hSpeedFactor*0.5,during=PlayerMovement,frames=3) ended applied=2 missing=0"
              && restored == "mark1probe hold: restored player.hSpeedFactor := 1" && twice.empty()
              && a.members["player.hSpeedFactor"].number == 1 && a.writes == 3,
              restored.empty() ? (ended.empty() ? "" : ended[0]) : restored);
    }
    {
        // A plain value; a nested call writes once; another self, never.
        Probe p;
        FakeAccess a;
        a.members["effect[182]"] = Num(7);
        p.StartHold(SetLever("hold effect[182] 0 during PlayerMovement"), 0, true, Num(7));
        p.Around(Row::PlayerMovement, 1, true, a, [&] { p.Around(Row::PlayerMovement, 1, true, a, [] {}); });
        FakeAccess other;
        other.members["effect[182]"] = Num(7);
        p.Around(Row::PlayerMovement, 2, false, other, [] {});
        check("hold/a_value_once_per_outer_call_and_only_for_the_local_player", a.writes == 1
              && a.members["effect[182]"].number == 0 && other.Untouched() && p.Applied().applied == 1 && p.HoldOwed());
        // Replaced by another lever: the adapter restores with force first.
        const std::string restored = p.RestoreHold(a, 3, true);
        p.SetLever(SetLever("stat 64 2"), 3);
        check("hold/a_new_lever_restores_it_first", restored == "mark1probe hold: restored effect[182] := 7"
              && a.members["effect[182]"].number == 7 && !p.HoldOwed());
        // `off` ends it too: the restore is then owed, not live.
        Probe q;
        FakeAccess b;
        b.members["pst[3]"] = Num(2);
        q.StartHold(SetLever("hold pst[3] 9 during TalentUse"), 0, true, Num(2));
        q.Around(Row::TalentUse, 0, true, b, [] {});
        q.Off();
        const std::string offRestore = q.RestoreHold(b, 1, false);
        check("hold/off_ends_it_and_the_restore_follows", offRestore == "mark1probe hold: restored pst[3] := 2"
              && b.members["pst[3]"].number == 2);
    }
    {
        // Nothing read at the start: nothing written, nothing to restore.
        Probe p;
        FakeAccess a;
        p.StartHold(SetLever("hold player.missing 1 during PlayerMovement"), 0, false, Value{});
        p.Around(Row::PlayerMovement, 0, true, a, [] {});
        const auto ended = p.Tick(600);
        // x<m> of a bool: refused the same way.
        Probe q;
        FakeAccess b;
        b.members["player.flag"] = Value{ true, 1 };
        q.StartHold(SetLever("hold player.flag x2 during PlayerMovement"), 0, true, Value{ true, 1 });
        q.Around(Row::PlayerMovement, 0, true, b, [] {});
        check("hold/an_unread_start_or_a_bool_multiple_writes_nothing", a.writes == 0 && p.Applied().missing == 1
              && p.RestoreHold(a, 601, true).empty() && ended.size() == 1 && b.writes == 0 && q.Applied().missing == 1
              && !q.HoldOwed());
    }

    // ---- the phase 2 watch scopes ------------------------------------------------
    {
        Probe p;
        p.StartWatch(Scope::Effect, 10, 0, false, Row::CheckTalentUse);
        p.ObserveWatch(0, { { "182", "0" } });
        const auto effect = p.ObserveWatch(1, { { "182", "1" } });
        Probe q;
        q.StartWatch(Scope::Pst, 10, 0, false, Row::CheckTalentUse);
        q.ObserveWatch(0, { { "[3]", "4" } });
        const auto pst = q.ObserveWatch(1, { { "[3]", "2" } });
        Probe r;
        r.StartWatch(Scope::Builtin, 10, 0, false, Row::CheckTalentUse);
        r.ObserveWatch(0, { { "speed", "0" }, { "alarm[0]", "-1" } });
        const auto builtin = r.ObserveWatch(1, { { "speed", "4" }, { "alarm[0]", "12" } });
        check("watch/effect_pst_array_and_builtin_name_members_as_targets", effect.size() == 1
              && effect[0] == "watch f=1 effect[182]=0->1" && pst.size() == 1 && pst[0] == "watch f=1 pst[3]=4->2"
              && builtin.size() == 2 && builtin[0] == "watch f=1 builtin.alarm[0]=-1->12" && builtin[1] == "watch f=1 builtin.speed=0->4",
              effect.empty() ? "" : effect[0]);
        // Each watched name parses back as a target that names it the same way.
        Target t;
        const auto back = [&](const std::string& text) {
            const Command c = parsed("hold " + text + " 1 during PlayerMovement");
            return c.form == Form::Hold && TargetText(c.lever.target) == text;
        };
        check("watch/a_watched_name_is_a_hold_target", back("effect[182]") && back("pst[3]") && back("builtin.speed")
              && back("builtin.alarm[0]") && back("input[4]") && back("player.hSpeedFactor") && back("pst.jumping")
              && PstArrayTarget(parsed("hold pst[3] 1 during TalentUse").lever.target)
              && !PstArrayTarget(parsed("hold pst.jumping 1 during TalentUse").lever.target));
        (void)t;
    }

    // ---- base ---------------------------------------------------------------------
    {
        check("base/key_and_definition", BaseKey(1700000000000LL, 4) == "0-0-1700000000000-4"
              && BaseJson(7) == "{\"w\":1,\"a\":779000,\"j\":0,\"b\":7,\"c\":0,\"o\":1}");
        BaseResult r;
        r.itemClass = 0;
        r.base = 7;
        r.built = true;
        r.name = "Iron Helmet";
        r.rarity = 1;
        r.sprite = 4210;
        r.spriteExists = true;
        BaseResult n;
        n.itemClass = 4;
        n.base = 3;
        n.why = "no-itemInfoStruct";
        check("base/line_built_and_not_built", BaseLine(r)
              == "mark1probe base: class=0 b=7 built=yes rarity=1 sprite=4210 sprite-exists=yes name=\"Iron Helmet\""
              && BaseLine(n) == "mark1probe base: class=4 b=3 built=no why=no-itemInfoStruct rarity=none sprite=none name=none",
              BaseLine(r) + " | " + BaseLine(n));
    }

    // ---- spawn -------------------------------------------------------------------
    {
        // The Mark 1 set's pieces, from Mark1Set.hpp's table: one word places
        // one piece, `set` all four in slot order; anything else none.
        const std::vector<ForgePact::Mark1Set::Piece> all = SpawnPieces("set");
        const std::vector<ForgePact::Mark1Set::Piece> one = SpawnPieces("boots");
        const bool pieces = all.size() == 4 && all[0] == ForgePact::Mark1Set::Piece::Helmet
            && all[1] == ForgePact::Mark1Set::Piece::Body && all[2] == ForgePact::Mark1Set::Piece::Boots
            && all[3] == ForgePact::Mark1Set::Piece::Gloves
            && one.size() == 1 && one[0] == ForgePact::Mark1Set::Piece::Boots && SpawnPieces("ring").empty();
        check("spawn/pieces_by_word", pieces);
        const ForgePact::Mark1Set::PieceRow& helmet = ForgePact::Mark1Set::RowOf(ForgePact::Mark1Set::Piece::Helmet);
        check("spawn/definition_is_the_rows_seed_and_base",
              PieceJson(helmet.seed, helmet.baseB) == "{\"w\":1,\"a\":777500,\"j\":0,\"b\":7,\"c\":0,\"o\":1}"
                  && BaseKey(1700000000000LL, ForgePact::Mark1Set::RowOf(ForgePact::Mark1Set::Piece::Gloves).itemType)
                         == "0-0-1700000000000-4",
              PieceJson(helmet.seed, helmet.baseB));
        const Command s1 = parsed("spawn SET");
        const Command s2 = parsed("spawn gloves");
        check("spawn/parse", s1.form == Form::Spawn && s1.piece == "set" && s2.form == Form::Spawn && s2.piece == "gloves");
        SpawnResult placed;
        placed.piece = "helmet";
        placed.built = true;
        placed.t = 0;
        placed.b = 7;
        placed.a = 777500;
        placed.dressed = true;
        placed.tier = 5;
        placed.ground = "ref:100234";
        SpawnResult refused = placed;
        refused.piece = "body";
        refused.dressed = false;
        refused.tier.reset();
        refused.ground.reset();
        refused.why = "LootGroundCreateFromItem-st=3";
        SpawnResult unbuilt;
        unbuilt.piece = "gloves";
        unbuilt.why = "InitItemFromJson-st=3-gave-undefined";
        check("spawn/line_placed_refused_and_not_built",
              SpawnLine(placed) == "mark1probe spawn: piece=helmet t=0 b=7 a=777500 dressed=yes tier=5 ground=ref:100234"
                  && SpawnLine(refused) == "mark1probe spawn: piece=body t=0 b=7 a=777500 dressed=no tier=? ground=none why=LootGroundCreateFromItem-st=3"
                  && SpawnLine(unbuilt) == "mark1probe spawn: piece=gloves built=no why=InitItemFromJson-st=3-gave-undefined",
              SpawnLine(placed) + " | " + SpawnLine(refused) + " | " + SpawnLine(unbuilt));
        // The tier sits right after `dressed=`, the dressing's SS 5 or whatever
        // the base kept, and `?` when info key 32 could not be read.
        SpawnResult based = placed;
        based.dressed = false;
        based.tier = 2;
        check("spawn/line_tier_after_dressed_question_mark_when_unread",
              SpawnLine(placed).find(" dressed=yes tier=5 ") != std::string::npos
                  && SpawnLine(based).find(" dressed=no tier=2 ") != std::string::npos
                  && SpawnLine(refused).find(" dressed=no tier=? ") != std::string::npos
                  && SpawnLine(unbuilt).find("tier=") == std::string::npos,
              SpawnLine(based));
    }

    // ---- heroic, pool, copies (the drops' forms) ------------------------------------
    {
        const Command h = parsed("heroic 10");
        const Command h20 = parsed("HEROIC 20");
        check("heroic/parse", h.form == Form::Heroic && h.count == 10 && h20.form == Form::Heroic && h20.count == 20);
        HeroicCall piece;
        piece.placed = true;
        piece.t = 2;
        piece.b = 0;
        piece.c = 0;
        piece.rarity = 6;   // the dressing's Satanic (Mark1Set.hpp's kRarity)
        piece.tier = 5;     // and its SS
        piece.name = "Mark 1 Boots";
        piece.isPiece = true;
        piece.piece = ForgePact::Mark1Set::Piece::Boots;
        HeroicCall other = piece;
        other.t = 5;
        other.b = 12;
        other.c = 1;
        other.rarity = 6;
        other.tier = 4;
        other.name = "Some Unique";
        other.isPiece = false;
        HeroicCall heroicUnique = other;
        heroicUnique.rarity = 9;
        HeroicCall unread = other;
        unread.rarity.reset();
        unread.tier.reset();
        HeroicCall nothing;
        nothing.why = "returned undefined";
        check("heroic/call_lines_placed_and_nothing",
              HeroicCallLine(3, piece) == "mark1probe heroic #3: placed t=2 b=0 c=0 rarity=6 tier=5 name=\"Mark 1 Boots\" piece=boots"
                  && HeroicCallLine(4, other) == "mark1probe heroic #4: placed t=5 b=12 c=1 rarity=6 tier=4 name=\"Some Unique\" piece=none"
                  && HeroicCallLine(5, nothing) == "mark1probe heroic #5: nothing placed (returned undefined)",
              HeroicCallLine(3, piece) + " | " + HeroicCallLine(4, other) + " | " + HeroicCallLine(5, nothing));
        check("heroic/call_line_tier_after_rarity_question_mark_when_unread",
              HeroicCallLine(6, unread) == "mark1probe heroic #6: placed t=5 b=12 c=1 rarity=? tier=? name=\"Some Unique\" piece=none"
                  && HeroicCallLine(3, piece).find(" rarity=6 tier=5 name=") != std::string::npos
                  && HeroicCallLine(5, nothing).find("tier=") == std::string::npos,
              HeroicCallLine(6, unread));
        HeroicTally tally;
        for (const HeroicCall& r : { piece, other, heroicUnique, nothing, unread, piece })
            tally.Add(r);
        check("heroic/summary_rarities_rising_and_pieces_by_recognition",
              tally.Line() == "mark1probe heroic: calls=6 placed=5 rarity=6:3,9:1 rarity-unread=1 pieces=helmet:0,body:0,boots:2,gloves:0 self=player"
                  && tally.Calls() == 6 && tally.Placed() == 5 && tally.Pieces(ForgePact::Mark1Set::Piece::Boots) == 2,
              tally.Line());
        check("heroic/a_fresh_summary_reads_none",
              HeroicTally().Line() == "mark1probe heroic: calls=0 placed=0 rarity=- rarity-unread=0 pieces=helmet:0,body:0,boots:0,gloves:0 self=player",
              HeroicTally().Line());
    }
    {
        namespace D = ForgePact::Mark1Drops;
        const Command bare = parsed("pool");
        const Command two = parsed("pool 2");
        const Command copies = parsed("copies 200");
        check("pool/parse_default_five_and_a_named_list",
              bare.form == Form::Pool && bare.count == 5 && two.form == Form::Pool && two.count == 2
                  && copies.form == Form::Copies && copies.count == 200 && parsed("copies 1").count == 1);
        auto e = [](double t, double sub, double b) { D::ListEntry x; x.triple = true; x.t = t; x.sub = sub; x.b = b; return x; };
        // Helmets: 0/3/10 Heroic twice, 0/3/11 Satanic; 0/0/85 Angelic (drawn past). Body
        // armor: 1/4/20 Heroic. Boots: 2/5/31 hidden. Gloves: 4/6/40 unread. Three uniques of
        // class 5, Heroic, Heroic and Unholy, and one entry that is not a triple.
        std::vector<D::ListEntry> list = { e(0, 3, 10), e(0, 3, 10), e(0, 3, 11), e(0, 0, 85), e(1, 4, 20), e(2, 5, 31),
                                           e(4, 6, 40), e(5, 0, 1), e(5, 0, 2), e(5, 0, 3), D::ListEntry{} };
        std::map<std::tuple<long long, long long, long long>, int> asked;
        auto verdict = [&](const D::Triple& k, int& rarity, bool& hidden) {
            ++asked[{ k.t, k.sub, k.b }];
            hidden = false;
            if (k.t == 4) return false;
            if (k.t == 0) rarity = k.b == 10 ? 9 : k.b == 11 ? 6 : 7;
            else if (k.t == 1) rarity = 9;
            else if (k.t == 2) { rarity = 9; hidden = true; }
            else rarity = k.b == 3 ? 10 : 9;
            return true;
        };
        const PoolSummary s = Pool(5, list, verdict);
        const std::vector<std::string> lines = PoolLines(s);
        bool once = true;
        for (const auto& [k, n] : asked) once = once && n == 1;
        check("pool/summary_droppable_histograms_and_what_is_drawn_past",
              lines.size() == 5
                  && lines[0] == "mark1probe pool: list=5 length=11 droppable=6 kept=6:1,9:5 refused=7:1,10:1 hidden=1 unread=1 other=1",
              lines.empty() ? std::string("no lines") : lines[0]);
        check("pool/each_piece_standin_by_the_drops_rule",
              lines.size() == 5
                  && lines[1] == "mark1probe pool: helmet class=0 standin=3:10@9x2 kept-triples=2 kept-entries=3"
                  && lines[2] == "mark1probe pool: body class=1 standin=4:20@9x1 kept-triples=1 kept-entries=1"
                  && lines[3] == "mark1probe pool: boots class=2 standin=none kept-triples=0 kept-entries=0"
                  && lines[4] == "mark1probe pool: gloves class=4 standin=none kept-triples=0 kept-entries=0",
              lines.size() == 5 ? lines[1] + " | " + lines[3] + " | " + lines[4] : std::string("not five lines"));
        check("pool/each_distinct_triple_asked_once", once && asked.size() == 9, std::to_string(asked.size()));
        check("copies/line", CopiesLine(200) == "mark1probe copies: each piece's stand-in is pushed 200 time(s) per roll, Heroic and unique (the player build pushes 1)",
              CopiesLine(200));
    }

    // ---- unique, pool kill (the kill route's forms) ------------------------------------
    {
        const Command u = parsed("unique 10");
        const Command u20 = parsed("UNIQUE 20");
        check("unique/parse_kill", u.form == Form::Unique && u.count == 10 && u20.form == Form::Unique && u20.count == 20);
        HeroicCall piece;
        piece.placed = true;
        piece.t = 0;
        piece.b = 7;
        piece.c = 0;
        piece.rarity = 6;
        piece.tier = 5;
        piece.name = "Mark 1 Helmet";
        piece.isPiece = true;
        piece.piece = ForgePact::Mark1Set::Piece::Helmet;
        HeroicCall other = piece;
        other.t = 4;
        other.b = 31;
        other.c = 1;
        other.rarity = 3;
        other.tier = 2;
        other.name = "Some Unique";
        other.isPiece = false;
        HeroicCall nothing;
        nothing.why = "returned undefined";
        check("unique/call_lines_for_the_kill_route",
              UniqueCallLine(1, piece) == "mark1probe unique #1: placed t=0 b=7 c=0 rarity=6 tier=5 name=\"Mark 1 Helmet\" piece=helmet"
                  && UniqueCallLine(2, other) == "mark1probe unique #2: placed t=4 b=31 c=1 rarity=3 tier=2 name=\"Some Unique\" piece=none"
                  && UniqueCallLine(3, nothing) == "mark1probe unique #3: nothing placed (returned undefined)"
                  && CallLine("heroic", 1, piece) == HeroicCallLine(1, piece),
              UniqueCallLine(1, piece) + " | " + UniqueCallLine(2, other));
        HeroicTally tally("unique");
        for (const HeroicCall& r : { piece, other, nothing, other })
            tally.Add(r);
        check("unique/summary_under_its_own_name_for_the_kill_route",
              tally.Line() == "mark1probe unique: calls=4 placed=3 rarity=3:2,6:1 rarity-unread=0 pieces=helmet:1,body:0,boots:0,gloves:0 self=player"
                  && HeroicTally("unique").Line().rfind("mark1probe unique: calls=0 placed=0 rarity=- ", 0) == 0,
              tally.Line());
    }
    {
        namespace D = ForgePact::Mark1Drops;
        const Command kill = parsed("pool kill");
        const Command killUpper = parsed("POOL KILL");
        check("pool/kill_parse", kill.form == Form::PoolKill && killUpper.form == Form::PoolKill && parsed("pool 4").form == Form::Pool);
        auto e = [](double t, double sub, double b) { D::ListEntry x; x.triple = true; x.t = t; x.sub = sub; x.b = b; return x; };
        // Helmets: 0/7/10 once on each of [0..4] (qualifies, n 1), 0/7/11 on [5] alone (n 0, the
        // stand-in). Body armor: 1/8/20 twice on [0], once on the rest (never). Boots: 2/9/30
        // Heroic (the ordinary roll refuses it). Gloves: 4/6/40 on [5] alone, unread.
        D::KillLists lists;
        for (int k = 0; k < 5; ++k) lists[(size_t)k] = { e(0, 7, 10), e(1, 8, 20), e(2, 9, 30), e(5, 0, (double)k) };
        lists[0].push_back(e(1, 8, 20));
        lists[0].push_back(D::ListEntry{});
        lists[5] = { e(0, 7, 11), e(4, 6, 40) };
        std::map<std::tuple<long long, long long, long long>, int> asked;
        auto verdict = [&](const D::Triple& k, int& rarity, bool& hidden) {
            ++asked[{ k.t, k.sub, k.b }];
            hidden = false;
            if (k.t == 4) return false;
            rarity = k.t == 2 ? 9 : 6;
            return true;
        };
        const std::vector<std::string> lines = PoolKillLines(PoolKill(lists, verdict));
        bool once = true;
        for (const auto& [k, n] : asked) once = once && n == 1;
        check("pool/kill_lengths_standins_and_counts",
              lines.size() == 5
                  && lines[0] == "mark1probe pool: kill lengths=6,4,4,4,4 unread=1"
                  && lines[1] == "mark1probe pool: helmet class=0 kill-standin=7:11@6x0 counts=0,0,0,0,0 candidates=2 kept-triples=2"
                  && lines[2] == "mark1probe pool: body class=1 kill-standin=none counts=- candidates=0 kept-triples=1"
                  && lines[3] == "mark1probe pool: boots class=2 kill-standin=none counts=- candidates=0 kept-triples=0"
                  && lines[4] == "mark1probe pool: gloves class=4 kill-standin=none counts=- candidates=0 kept-triples=0",
              lines.size() == 5 ? lines[0] + " | " + lines[1] + " | " + lines[2] : std::string("not five lines"));
        check("pool/kill_each_distinct_triple_asked_once", once && asked.size() == 5, std::to_string(asked.size()));
    }

    // ---- killgate (Mod live procedure 6) -------------------------------------------------
    {
        // Baseline: the lever off. Every call is the game's own, nothing is written and
        // nothing re-entered, and only the counters move.
        Killgate gate;
        FakeLoadDrops ops;
        const std::vector<double> before = ops.chances;
        for (int t : { 4, 6, 10, 45, 11 }) {
            ops.type = t;
            gate.Around(t, true, 1, 7, ops);
        }
        const KillgateCounts& k = gate.Counts();
        check("killgate/baseline_off_every_call_passes_through_untouched",
              !gate.On() && ops.gameCalls == 5 && ops.reentries == 0 && ops.writes == 0 && ops.chances == before
                  && ops.log.size() == 5 && ops.log[3] == "game t=45 c45=0" && ops.uniques == 0,
              std::to_string(ops.gameCalls) + " calls, " + std::to_string(ops.writes) + " writes");
        check("killgate/baseline_off_only_the_counters_move",
              k.loaddrops == 5 && k.arrays == 1 && k.c45Nonzero == 0 && k.c45Max == 0 && k.game45 == 1 && k.raised == 0
                  && k.reentered == 0 && k.uniqueDuring == 0 && k.threw == 0 && ops.reads == 1,
              gate.StatLine(KillgateHook::Detoured));
    }
    {
        // Baseline: type 45's vanilla chance, read once per array before anything else; an
        // id seen again in a later frame is a new array (one DropItem is one frame's call).
        Killgate gate;
        FakeLoadDrops ops;
        ops.type = 4;
        gate.Around(4, true, 1, 7, ops);
        gate.Around(6, true, 1, 7, ops);
        ops.chances[45] = 3;
        gate.Around(4, true, 2, 7, ops);
        ops.chances[45] = 0;
        gate.Around(4, true, 1, 8, ops);
        gate.Around(4, false, 0, 8, ops);   // no array: counted as a call, never as an array
        const KillgateCounts& k = gate.Counts();
        check("killgate/baseline_off_vanilla_45_read_once_per_array",
              k.loaddrops == 5 && k.arrays == 3 && k.c45Nonzero == 1 && k.c45Max == 3 && ops.reads == 3 && ops.writes == 0
                  && ops.gameCalls == 5,
              gate.StatLine(KillgateHook::Detoured));
    }
    {
        // Baseline: a hook that is not the inline detour refuses the lever, nothing changed;
        // the inline detour is the negative control that it is not refused outright.
        Killgate gate;
        const std::string tableOnly = gate.Set(true, KillgateHook::TableOnly);
        const bool offAfterTable = !gate.On();
        const std::string none = gate.Set(true, KillgateHook::None);
        const std::string refused = gate.Set(false, KillgateHook::Refused);
        FakeLoadDrops ops;
        ops.type = 4;
        gate.Around(4, true, 1, 1, ops);
        const bool untouched = ops.writes == 0 && ops.reentries == 0 && ops.gameCalls == 1;
        const std::string on = gate.Set(true, KillgateHook::Detoured);
        check("killgate/baseline_a_hook_not_inline_refuses_nothing_changed",
              offAfterTable && untouched
                  && tableOnly == "mark1probe killgate: refused - the LoadDrops hook is table-only, and only the inline detour sees DropItem's direct call; the lever stays off, nothing changed"
                  && none == "mark1probe killgate: refused - the LoadDrops hook is none, and only the inline detour sees DropItem's direct call; the lever stays off, nothing changed"
                  && refused.find("the LoadDrops hook is refused") != std::string::npos && gate.On()
                  && on == "mark1probe killgate: lever on - the first LoadDrops call of each kill opens type 45 once, inside the game's own call"
                  && gate.Set(true, KillgateHook::TableOnly).find("the lever stays on, nothing changed") != std::string::npos && gate.On(),
              tableOnly + " | " + none + " | " + on);
        check("killgate/baseline_hook_route_from_the_install",
              KillgateHookOf(false, true, true) == KillgateHook::Detoured && KillgateHookOf(false, true, false) == KillgateHook::TableOnly
                  && KillgateHookOf(true, false, false) == KillgateHook::Refused && KillgateHookOf(false, false, false) == KillgateHook::None
                  && std::string(KillgateHookText(KillgateHook::Detoured)) == "detoured"
                  && std::string(KillgateHookText(KillgateHook::TableOnly)) == "table-only"
                  && std::string(KillgateHookText(KillgateHook::Refused)) == "refused"
                  && std::string(KillgateHookText(KillgateHook::None)) == "none");
    }
    {
        // Baseline: on, then off again: vanilla once more.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        gate.Set(false, KillgateHook::Detoured);
        FakeLoadDrops ops;
        ops.type = 45;
        gate.Around(45, true, 1, 1, ops);
        ops.type = 4;
        gate.Around(4, true, 2, 1, ops);
        check("killgate/baseline_lever_off_again_is_vanilla",
              !gate.On() && ops.writes == 0 && ops.reentries == 0 && ops.gameCalls == 2 && gate.Counts().raised == 0
                  && gate.Counts().reentered == 0 && gate.Counts().loaddrops == 2 && gate.Counts().arrays == 2);
    }
    {
        // Target: the game's own type-45 call on a fresh array runs with chances[45] opened
        // for that call, and the vanilla value goes back after it.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops ops;
        ops.type = 45;
        gate.Around(45, true, 1, 1, ops);
        const KillgateCounts& k = gate.Counts();
        check("killgate/target_game_45_call_raised_and_put_back",
              ops.log == std::vector<std::string>{ "write 100000", "game t=45 c45=100000", "write 0" } && ops.chances[45] == 0
                  && ops.gameCalls == 1 && ops.reentries == 0 && k.raised == 1 && k.reentered == 0 && k.game45 == 1
                  && k.uniqueDuring == 1 && ops.uniques == 1,
              ops.log.empty() ? std::string("no log") : ops.log[0]);
    }
    {
        // Target: the first game call on a fresh array of another type runs untouched, then
        // the trampoline once more at type 45 with the chance opened, put back after. A later
        // call on the same array, and a game type-45 call on it, pass untouched.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops ops;
        ops.chances[45] = 7;   // a vanilla chance that is not 0 goes back as it was
        ops.type = 4;
        gate.Around(4, true, 1, 1, ops);
        const std::vector<std::string> first = ops.log;
        ops.log.clear();
        ops.type = 6;
        gate.Around(6, true, 1, 1, ops);
        ops.type = 45;
        gate.Around(45, true, 1, 1, ops);
        const KillgateCounts& k = gate.Counts();
        check("killgate/target_first_call_reenters_45_once_and_puts_back",
              first == std::vector<std::string>{ "game t=4 c45=7", "write 100000", "reenter t=45 c45=100000", "write 7" }
                  && k.reentered == 1 && k.raised == 0 && k.uniqueDuring == 1 && k.c45Nonzero == 1 && k.c45Max == 7,
              first.empty() ? std::string("no log") : first[0] + " .. " + first.back());
        check("killgate/target_later_calls_on_a_handled_array_pass_untouched",
              ops.log == std::vector<std::string>{ "game t=6 c45=7", "game t=45 c45=7" } && ops.chances[45] == 7
                  && ops.reentries == 1 && ops.gameCalls == 3 && k.game45 == 1 && k.arrays == 1 && k.loaddrops == 3,
              ops.log.empty() ? std::string("no log") : ops.log[0]);
        // The next kill (a new array) is opened again, once.
        ops.log.clear();
        ops.type = 4;
        gate.Around(4, true, 2, 1, ops);
        gate.Around(4, true, 2, 1, ops);
        check("killgate/target_each_new_array_opened_once",
              gate.Counts().reentered == 2 && ops.reentries == 2 && ops.log.size() == 5 && ops.log.back() == "game t=4 c45=7");
    }
    {
        // Target: a throw inside the opened call puts the chance back and counts threw=. The
        // game's own (raised) call's throw is passed on; the re-entry's, ours, is not.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops ops;
        ops.type = 45;
        ops.throwOnGame = true;
        bool passedOn = false;
        try { gate.Around(45, true, 1, 1, ops); } catch (const std::runtime_error&) { passedOn = true; }
        const bool raisedBack = ops.chances[45] == 0 && ops.log.back() == "write 0";
        ops.throwOnGame = false;
        ops.throwOnReentry = true;
        ops.type = 4;
        bool escaped = false;
        try { gate.Around(4, true, 2, 1, ops); } catch (...) { escaped = true; }
        const KillgateCounts& k = gate.Counts();
        check("killgate/target_a_throw_puts_the_chance_back_and_counts_threw",
              passedOn && raisedBack && !escaped && ops.chances[45] == 0 && ops.log.back() == "write 0" && k.threw == 2
                  && k.raised == 1 && k.reentered == 1 && ops.gameCalls == 2,
              gate.StatLine(KillgateHook::Detoured));
        // After both throws the gate is not left nested: the next array is opened as usual.
        ops.throwOnReentry = false;
        gate.Around(4, true, 3, 1, ops);
        check("killgate/target_not_left_nested_after_a_throw", gate.Counts().reentered == 2 && ops.chances[45] == 0);
    }
    {
        // Target: a LoadDrops call nested inside an opened one (another array) is the game's
        // own, untouched: a re-entry is never nested inside another.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops ops;
        FakeLoadDrops inner;
        inner.type = 4;
        ops.duringReentry = [&] { gate.Around(4, true, 9, 1, inner); };
        ops.type = 4;
        gate.Around(4, true, 1, 1, ops);
        check("killgate/target_a_call_nested_in_an_opened_one_passes_untouched",
              inner.gameCalls == 1 && inner.writes == 0 && inner.reentries == 0 && inner.reads == 0 && ops.reentries == 1
                  && gate.Counts().reentered == 1 && gate.Counts().loaddrops == 2);
    }
    {
        // Target: an array whose chances[45] cannot be read, or written, is passed through.
        Killgate gate;
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops unread;
        unread.readable = false;
        unread.type = 4;
        gate.Around(4, true, 1, 1, unread);
        FakeLoadDrops locked;
        locked.writable = false;
        locked.type = 45;
        gate.Around(45, true, 2, 1, locked);
        locked.type = 4;
        gate.Around(4, true, 3, 1, locked);
        check("killgate/target_unreadable_or_unwritable_chance_passes_through",
              unread.writes == 0 && unread.reentries == 0 && unread.gameCalls == 1 && locked.gameCalls == 2 && locked.reentries == 0
                  && locked.chances[45] == 0 && gate.Counts().raised == 0 && gate.Counts().reentered == 0,
              gate.StatLine(KillgateHook::Detoured));
    }
    {
        // The stat line's exact text: a fresh one, and one after a raise and a re-entry.
        Killgate gate;
        const std::string fresh = gate.StatLine(KillgateHook::None);
        gate.Set(true, KillgateHook::Detoured);
        FakeLoadDrops ops;
        ops.chances[45] = 2.5;
        ops.type = 45;
        gate.Around(45, true, 1, 1, ops);
        ops.type = 4;
        gate.Around(4, true, 2, 1, ops);
        check("killgate/stat_line",
              fresh == "mark1probe killgate: lever=off hook=none loaddrops=0 arrays=0 c45-nonzero=0 c45-max=0 game45=0 raised=0 reentered=0 unique-during=0 threw=0"
                  && gate.StatLine(KillgateHook::Detoured) == "mark1probe killgate: lever=on hook=detoured loaddrops=2 arrays=2 c45-nonzero=2 c45-max=2.5 game45=1 raised=1 reentered=1 unique-during=2 threw=0",
              fresh + " | " + gate.StatLine(KillgateHook::Detoured));
        check("killgate/off_reply",
              gate.Set(false, KillgateHook::Detoured) == "mark1probe killgate: lever off - every LoadDrops call is the game's own (still counted)");
    }
    {
        const Command bare = parsed("killgate");
        const Command on = parsed("killgate 1");
        const Command off = parsed("KILLGATE 0");
        check("killgate/parse", bare.form == Form::Killgate && on.form == Form::KillgateSet && on.count == 1
                                   && off.form == Form::KillgateSet && off.count == 0);
    }

    // ---- draws (Mod live procedure 7) ---------------------------------------------------
    {
        // Baseline: the watch off. Nothing is counted, no scope opens, a DropItem call is not
        // taken (the adapter then calls the game's DropItem as it is), and the lever, though
        // set, pushes nothing.
        Draws w;
        const std::string fresh = w.StatusLine(DrawRoutes{});
        const bool begun = w.BeginCall(12, DrawArg7::Undefined);
        const bool entered = w.EnterRoll(DrawScope::DropUnique);
        w.NoteTier(true, 3);
        w.NoteRead(true, 0, 7, 10);
        w.NotePick(true, 7, 10);
        const std::string line = w.NoteBuilt(BuiltOf({ 0, 7, 10, 1, "Off" }), DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        w.NoteRetsat();
        const DrawCounts& c = w.Counts();
        bool zero = c.calls == 0 && c.reads == 0 && c.builds == 0 && c.retsatCalls == 0 && c.picks == 0 && c.tierUnread == 0;
        for (uint64_t n : c.tiers) zero = zero && n == 0;
        for (uint64_t n : c.scopes) zero = zero && n == 0;
        check("draws/baseline_off_nothing_counted", !w.On() && !begun && !entered && line.empty() && zero
                                                        && w.Current() == DrawScope::Outside && w.StatusLine(DrawRoutes{}) == fresh,
              w.StatusLine(DrawRoutes{}));
        check("draws/baseline_off_status_line_exact",
              fresh == "mark1probe draws: watch=off dropitem=not-installed sattier=not-installed retsat=not-installed calls=0 argc=- "
                       "arg7=undefined:0,list:0,other:0 tiers=0,0,0,0,0,0 tier-unread=0 reads=0 builds=0 normal=0 draw=0 "
                       "dropunique=0 angelic=0 heroic=0 outside=0 on-k=0 off-k=0 retsat-calls=0 push=off push-side=- picks=0 pick-builds=0 "
                       "push-held=0 push-skipped=0 pushed-reads=0 anomalies=0",
              fresh);
        const std::string push = w.SetPush(100, false, false);
        check("draws/baseline_off_the_lever_pushes_nothing",
              w.Push() == 100 && !w.ShouldPush(false, false) && !w.Pushing()
                  && push == "mark1probe draws push: c=100 - while the watch is on, each DropItem call pushes 100 copies of each Mark 1 kill stand-in onto each of lootListUnique[0..5]; nothing is rewritten",
              push);
        // A route other than the inline detour refuses the watch and changes nothing.
        DrawRoutes table;
        table.dropitem = "TABLE-ONLY";
        table.sattier = "detoured";
        table.retsat = "not-found";
        DrawRoutes held = table;
        held.dropitem = "held-by-gambaprobe";
        const std::string refusedTable = w.SetOn(true, table);
        const std::string refusedHeld = w.SetOn(true, held);
        check("draws/baseline_a_route_not_inline_refuses_the_watch",
              !w.On()
                  && refusedTable == "mark1probe draws: refused - dropitem=TABLE-ONLY sattier=detoured retsat=not-found (only DropManager's inline DropItem detour sees the game's calls, and a probe holding its slot is named); the watch stays off"
                  && refusedHeld.rfind("mark1probe draws: refused - dropitem=held-by-gambaprobe ", 0) == 0,
              refusedTable + " | " + refusedHeld);
    }
    {
        // Baseline: the lever's book with an empty plan, or an unreadable list, writes nothing.
        DrawPushBook book;
        FakeDrawLists lists;
        lists.lists[0] = { { 0, 7, 10 } };
        std::vector<std::string> lines;
        const DrawPushResult empty = book.Push({}, lists, lines);
        lists.unreadable = 3;
        const DrawPushResult unread = book.Push({ ForgePact::Mark1Drops::PushEntry{ ForgePact::Mark1Set::Piece::Helmet, { 0, 7, 10 }, 0, 5 } },
                                                lists, lines);
        check("draws/baseline_push_with_nothing_planned_or_a_list_unread_writes_nothing",
              empty == DrawPushResult::None && unread == DrawPushResult::None && lists.adds == 0 && lists.deletes == 0
                  && book.Pushed() == 0 && lines.empty() && lists.lists[0].size() == 1);
    }
    {
        // Target: each scope classes a build by the innermost one open.
        Draws w;
        const std::string on = w.SetOn(true, DetouredRoutes());
        DrawBuild(w, { 0, 1, 2, 1, "Shop" }, std::nullopt);                    // no call: outside
        w.BeginCall(12, DrawArg7::Undefined);
        DrawBuild(w, { 0, 7, 10, 1, "Drawn" }, std::nullopt);                   // draw
        w.EnterRoll(DrawScope::DropUnique);
        DrawBuild(w, { 1, 8, 20, 1, "Rolled" }, std::nullopt);                  // dropunique
        w.EnterRoll(DrawScope::Angelic);
        DrawBuild(w, { 3, 8, 21, 1, "Angel" }, std::nullopt);                   // angelic, innermost
        w.LeaveRoll();
        w.LeaveRoll();
        w.EnterRoll(DrawScope::Heroic);
        DrawBuild(w, { 2, 9, 30, 1, "Hero" }, std::nullopt);                    // heroic
        w.LeaveRoll();
        DrawBuild(w, { 4, 0, 99, 0, "Normal" }, std::nullopt);                  // normal, c 0
        w.NoteBuilt(BuiltOf({ 4, 0, 98, 2, "Other c" }), std::nullopt);         // neither
        w.EndCall();
        const DrawCounts& c = w.Counts();
        check("draws/target_each_scope_classes_a_build",
              on == "mark1probe draws: watch=on dropitem=detoured sattier=detoured retsat=detoured"
                  && c.scopes[(size_t)DrawScope::Outside] == 1 && c.scopes[(size_t)DrawScope::Draw] == 1
                  && c.scopes[(size_t)DrawScope::DropUnique] == 1 && c.scopes[(size_t)DrawScope::Angelic] == 1
                  && c.scopes[(size_t)DrawScope::Heroic] == 1 && c.normal == 1 && c.builds == 7 && c.calls == 1
                  && w.Current() == DrawScope::Outside,
              w.StatusLine(DetouredRoutes()));
        // A nested DropItem is its own draw scope; ending the outer one closes a roll left open.
        w.BeginCall(12, DrawArg7::List);
        w.EnterRoll(DrawScope::Angelic);
        w.BeginCall(10, DrawArg7::Other);
        const DrawScope nested = w.Current();
        w.EndCall();
        const DrawScope back = w.Current();
        w.EndCall();
        check("draws/target_nested_call_and_a_roll_left_open",
              nested == DrawScope::Draw && back == DrawScope::Angelic && w.Current() == DrawScope::Outside && w.Counts().calls == 3
                  && w.Counts().argcMin == 10 && w.Counts().argcMax == 12 && w.Counts().arg7[0] == 1 && w.Counts().arg7[1] == 1
                  && w.Counts().arg7[2] == 1,
              w.StatusLine(DetouredRoutes()));
    }
    {
        // Target: k, on-k and off-k follow the recorded tier.
        Draws w;
        w.SetOn(true, DetouredRoutes());
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteTier(true, 3);
        const std::string onK = DrawBuild(w, { 0, 7, 10, 1, "On k" }, DrawListCounts{ 0, 0, 0, 2, 0, 0 }, 2);
        w.EndCall();
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteTier(true, 2);
        const std::string offK = DrawBuild(w, { 0, 7, 10, 1, "Off k" }, DrawListCounts{ 0, 0, 0, 2, 0, 0 });
        w.EndCall();
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteTier(true, 7.5);   // not a whole 0..5: unread, k -
        const std::string unread = DrawBuild(w, { 0, 7, 10, 1, "No k" }, DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        w.EndCall();
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteTier(true, 0);
        w.NoteRead(true, 0, 7, 10);
        w.NotePick(true, 7, 11);   // the pick names another pair: untyped, no lists, no k count
        const std::string untyped = w.NoteBuilt(BuiltOf({ 0, 7, 11, 1, "Untyped" }), DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        w.EndCall();
        // Outside a call there is no k; a dropunique build in a call carries its call's k but
        // counts on neither.
        const std::string outside = DrawBuild(w, { 0, 7, 10, 1, "Out" }, DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteTier(true, 4);
        w.EnterRoll(DrawScope::DropUnique);
        const std::string inRoll = DrawBuild(w, { 0, 7, 10, 1, "Rolled" }, DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        w.LeaveRoll();
        w.EndCall();
        const DrawCounts& c = w.Counts();
        check("draws/target_k_on_k_and_off_k_follow_the_tier",
              c.onK == 1 && c.offK == 1 && c.tiers[3] == 1 && c.tiers[2] == 1 && c.tiers[0] == 1 && c.tiers[4] == 1
                  && c.tierUnread == 1 && c.reads == 8,
              w.StatusLine(DetouredRoutes()));
        check("draws/target_built_lines_exact",
              onK == "mark1probe draws built: \"On k\" t=0 b=10 rarity=6 tier=5 scope=draw k=3 lists=0,0,0,2,0,0 reads=3"
                  && offK == "mark1probe draws built: \"Off k\" t=0 b=10 rarity=6 tier=5 scope=draw k=2 lists=0,0,0,2,0,0 reads=1"
                  && unread == "mark1probe draws built: \"No k\" t=0 b=10 rarity=6 tier=5 scope=draw k=- lists=1,1,1,1,1,1 reads=1"
                  && untyped == "mark1probe draws built: \"Untyped\" t=0 b=11 rarity=6 tier=5 scope=draw k=0 lists=- reads=1"
                  && outside == "mark1probe draws built: \"Out\" t=0 b=10 rarity=6 tier=5 scope=outside k=- lists=1,1,1,1,1,1 reads=1"
                  && inRoll == "mark1probe draws built: \"Rolled\" t=0 b=10 rarity=6 tier=5 scope=dropunique k=4 lists=1,1,1,1,1,1 reads=1",
              onK + " | " + offK + " | " + unread + " | " + untyped + " | " + outside + " | " + inRoll);
        // The query names the triple only when the build's class and b agree with the pick.
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteRead(true, 0, 7, 10);
        w.NotePick(true, 7, 10);
        const DrawBuildQuery agree = w.Query(BuiltOf({ 0, 7, 10, 1, "A" }));
        const DrawBuildQuery other = w.Query(BuiltOf({ 1, 7, 10, 1, "B" }));
        const DrawBuildQuery normal = w.Query(BuiltOf({ 0, 7, 10, 0, "C" }));
        w.EndCall();
        check("draws/target_query_names_the_picked_triple",
              agree.unique && agree.haveTriple && agree.triple.t == 0 && agree.triple.sub == 7 && agree.triple.b == 10
                  && other.unique && !other.haveTriple && !normal.unique && !normal.haveTriple);
    }
    {
        // Target: the lever's plan pushes c of each kill stand-in onto each of [0..5] and the
        // removal takes exactly that off; the picks and pick-builds of a pushed stand-in.
        namespace D = ForgePact::Mark1Drops;
        D::KillResolution r;
        r.standIns[0] = D::StandIn{ true, { 0, 7, 11 }, 6, 0 };
        r.standIns[2] = D::StandIn{ true, { 2, 9, 31 }, 6, 1 };
        const std::vector<D::PushEntry> plan = DrawPushPlan(r, 3);
        FakeDrawLists lists;
        for (int k = 0; k < 6; ++k) lists.lists[(size_t)k] = { { 0, 7, 10 }, { 2, 9, 31 } };
        Draws w;
        w.SetOn(true, DetouredRoutes());
        w.SetPush(3, false, false);
        w.BeginCall(12, DrawArg7::Undefined);
        const bool should = w.ShouldPush(false, false);
        DrawPushBook book;
        std::vector<std::string> lines;
        const DrawPushResult pushed = book.Push(plan, lists, lines);
        w.NotePushed(book.Entries());
        bool grown = book.Before(0) == 2 && book.Before(5) == 2 && book.Before(6) == -1;
        for (const auto& l : lists.lists) grown = grown && l.size() == 8;
        // A nested DropItem rides on the outer call's entries.
        w.BeginCall(12, DrawArg7::Undefined);
        const bool nestedShould = w.ShouldPush(false, false);
        w.EndCall();
        w.NoteTier(true, 1);
        const std::string pick = DrawBuild(w, { 0, 7, 11, 1, "Stand-in" }, DrawListCounts{ 0, 0, 0, 0, 0, 0 });
        DrawBuild(w, { 0, 7, 10, 1, "Game's" }, DrawListCounts{ 1, 1, 1, 1, 1, 1 });
        // A pushed pair picked inside a nested roll is no draw pick.
        w.EnterRoll(DrawScope::DropUnique);
        w.NoteRead(true, 2, 9, 31);
        w.NotePick(true, 9, 31);
        w.LeaveRoll();
        const std::vector<std::string> removed = book.Remove(lists);
        w.EndCall();
        bool back = true;
        for (const auto& l : lists.lists) back = back && l.size() == 2;
        const DrawCounts& c = w.Counts();
        check("draws/target_push_plan_c_of_each_stand_in",
              plan.size() == 2 && plan[0].copies == 3 && plan[0].triple.b == 11 && plan[1].triple.b == 31 && plan[1].copies == 3
                  && DrawPushPlan(r, 0).empty() && DrawPushPlan(r, 900)[0].copies == 400);
        check("draws/target_push_onto_each_list_and_exactly_that_off",
              should && pushed == DrawPushResult::Held && grown && lists.adds == 36 && removed.empty() && back && lists.deletes == 36
                  && !nestedShould && !w.Pushing() && book.Pushed() == 0,
              std::to_string(lists.adds) + " adds, " + std::to_string(lists.deletes) + " deletes");
        check("draws/target_picks_and_pick_builds_of_a_pushed_stand_in",
              c.picks == 1 && c.pickBuilds == 1 && c.onK == 1 && c.offK == 0
                  && pick == "mark1probe draws built: \"Stand-in\" t=0 b=11 rarity=6 tier=5 scope=draw k=1 lists=0,0,0,0,0,0 reads=1",
              w.StatusLine(DetouredRoutes()));
    }
    {
        // Target: the tail rule. A list the game changed during the call is left as found and
        // named; an add that throws takes the partial push off again; a push its tail does not
        // show comes off too.
        namespace D = ForgePact::Mark1Drops;
        const std::vector<D::PushEntry> plan = { D::PushEntry{ ForgePact::Mark1Set::Piece::Helmet, { 0, 7, 11 }, 0, 2 } };
        FakeDrawLists lists;
        DrawPushBook book;
        std::vector<std::string> lines;
        const DrawPushResult pushed = book.Push(plan, lists, lines);
        lists.lists[4].push_back({ 9, 9, 9 });   // the game added an entry after ours
        const std::vector<std::string> removed = book.Remove(lists);
        check("draws/target_a_list_changed_during_the_call_is_left_as_found",
              pushed == DrawPushResult::Held && removed.size() == 1 && lists.lists[4].size() == 3 && lists.lists[0].empty()
                  && removed[0] == "mark1probe draws push: lootListUnique[4] changed during the call, left as found (size 0 + 2 pushed, now 3)",
              removed.empty() ? std::string("none") : removed[0]);
        FakeDrawLists throwing;
        throwing.throwAfter = 5;
        std::vector<std::string> threwLines;
        const DrawPushResult threw = book.Push(plan, throwing, threwLines);
        bool empty = true;
        for (const auto& l : throwing.lists) empty = empty && l.empty();
        check("draws/target_an_add_that_throws_comes_off_again",
              threw == DrawPushResult::Threw && throwing.adds == 5 && throwing.deletes == 5 && empty && threwLines.empty()
                  && book.Pushed() == 0
                  && DrawPushRefusedLine(threw) == "mark1probe draws push: an add threw - taken off again; this call carries no stand-in"
                  && DrawPushRefusedLine(DrawPushResult::NotHeld) == "mark1probe draws push: not visible on every list's tail - taken off again; calls carry no stand-in until a push reads back");
    }
    {
        // The lever refuses while either feature that pushes onto the same lists is on; off
        // always goes through. The watch's on and off replies, and the status line after use.
        Draws w;
        const std::string mark1 = w.SetPush(50, true, false);
        const std::string elf = w.SetPush(50, false, true);
        w.SetPush(50, false, false);
        const std::string stays = w.SetPush(20, false, true);
        const std::string off = w.SetPush(0, true, true);
        check("draws/target_push_refused_while_mark1_or_pocket_elf_is_on",
              mark1 == "mark1probe draws push: refused - Mark 1 drops are switched on (their push shares these lists); the lever stays off"
                  && elf == "mark1probe draws push: refused - Pocket Elf is switched on (its push shares these lists); the lever stays off"
                  && stays == "mark1probe draws push: refused - Pocket Elf is switched on (its push shares these lists); the lever stays 50"
                  && off == "mark1probe draws push: off - nothing is pushed" && w.Push() == 0,
              mark1 + " | " + elf + " | " + stays + " | " + off);
        w.SetOn(true, DetouredRoutes());
        w.SetPush(7, false, false);
        w.BeginCall(12, DrawArg7::List);
        w.NoteTier(true, 5);
        DrawBuild(w, { 0, 7, 10, 1, "X" }, DrawListCounts{ 0, 0, 0, 0, 0, 3 });
        w.NoteAnomaly();
        w.EndCall();
        w.NoteRetsat();
        DrawRoutes routes = DetouredRoutes();
        routes.retsat = "not-found";
        const std::string status = w.StatusLine(routes);
        const std::string offReply = w.SetOn(false, routes);
        w.BeginCall(12, DrawArg7::List);
        check("draws/target_status_line_and_off_reply",
              status == "mark1probe draws: watch=on dropitem=detoured sattier=detoured retsat=not-found calls=1 argc=12..12 "
                        "arg7=undefined:0,list:1,other:0 tiers=0,0,0,0,0,1 tier-unread=0 reads=1 builds=1 normal=0 draw=1 "
                        "dropunique=0 angelic=0 heroic=0 outside=0 on-k=1 off-k=0 retsat-calls=1 push=7 push-side=ss-first picks=0 "
                        "pick-builds=0 push-held=0 push-skipped=0 pushed-reads=0 anomalies=1"
                  && offReply == "mark1probe draws: watch=off dropitem=detoured sattier=detoured retsat=not-found - counting and the push stopped; the hooks stay"
                  && w.Counts().calls == 1 && !w.On(),
              status + " | " + offReply);
    }
    {
        // The built lines stop at 200 a session; the counters go on.
        Draws w;
        w.SetOn(true, DetouredRoutes());
        int lines = 0;
        for (int i = 0; i < 205; ++i)
            if (!DrawBuild(w, { 0, 7, 10, 1, "Many" }, std::nullopt).empty()) ++lines;
        check("draws/target_built_lines_capped_counters_not", lines == 200 && w.Counts().scopes[(size_t)DrawScope::Outside] == 205);
    }
    {
        const Command bare = parsed("draws");
        const Command on = parsed("draws on");
        const Command off = parsed("DRAWS OFF");
        const Command push = parsed("draws push 100");
        const Command pushOff = parsed("draws push off");
        const Command poolDraw = parsed("pool draw");
        check("draws/parse",
              bare.form == Form::Draws && on.form == Form::DrawsSet && on.count == 1 && off.form == Form::DrawsSet && off.count == 0
                  && push.form == Form::DrawsPush && push.count == 100 && pushOff.form == Form::DrawsPush && pushOff.count == 0
                  && poolDraw.form == Form::PoolDraw && parsed("draws push 400").count == 400
                  && why("draws maybe") == "usage: draws [on|off|push <c> [ss|below]|push off]"
                  && why("draws push") == "usage: draws [on|off|push <c> [ss|below]|push off]"
                  && why("draws push 0") == "usage: draws push <c> [ss|below]|off, c a whole number 1..400, not 0"
                  && why("draws push 401") == "usage: draws push <c> [ss|below]|off, c a whole number 1..400, not 401"
                  && why("draws push 5 ss below") == "usage: draws [on|off|push <c> [ss|below]|push off]"
                  && why("pool draw 2") == "usage: pool [<k>|kill|draw], k a sub-list 0..5");
    }

    // ---- draws: the lever's push counters (Mod live procedure 7, Live 1b) -----------------
    {
        // Baseline: the watch on and the push off. Calls (nested ones too), reads, typed picks
        // and builds leave push-held, push-skipped and pushed-reads at 0, and no call pushes.
        Draws w;
        w.SetOn(true, DetouredRoutes());
        bool anyShould = false;
        for (int i = 0; i < 3; ++i) {
            w.BeginCall(12, DrawArg7::Undefined);
            anyShould = anyShould || w.ShouldPush(false, false);
            w.NoteTier(true, 2);
            DrawBuild(w, { 0, 7, 10, 1, "Game's" }, DrawListCounts{ 0, 0, 1, 0, 0, 0 }, 2);
            w.BeginCall(12, DrawArg7::Undefined);
            anyShould = anyShould || w.ShouldPush(false, false);
            w.NoteRead(true, 0, 7, 10);
            w.EndCall();
            w.EndCall();
        }
        const DrawCounts& c = w.Counts();
        const std::string status = w.StatusLine(DetouredRoutes());
        check("draws/baseline_push_counters_stay_zero_without_a_push",
              !anyShould && c.calls == 6 && c.reads == 12 && c.onK == 3 && c.picks == 0 && c.pickBuilds == 0 && c.pushHeld == 0
                  && c.pushSkipped == 0 && c.pushedReads == 0
                  && status.find(" push=off push-side=- picks=0 pick-builds=0 push-held=0 push-skipped=0 pushed-reads=0 anomalies=0")
                         != std::string::npos,
              status);
    }
    {
        // Target: a push that reads back Held is one push-held per outermost call; the nested
        // DropItem riding on its entries adds none (noted or not), and push-skipped stays 0.
        namespace D = ForgePact::Mark1Drops;
        const std::vector<D::PushEntry> plan = { D::PushEntry{ ForgePact::Mark1Set::Piece::Helmet, { 0, 7, 10 }, 1, 2 } };
        FakeDrawLists lists;
        DrawPushBook book;
        Draws w;
        w.SetOn(true, DetouredRoutes());
        w.SetPush(2, false, false);
        bool nestedShould = false;
        for (int i = 0; i < 2; ++i) {
            w.BeginCall(12, DrawArg7::Undefined);
            std::vector<std::string> lines;
            if (w.ShouldPush(false, false) && book.Push(plan, lists, lines) == DrawPushResult::Held) w.NotePushed(book.Entries());
            w.BeginCall(12, DrawArg7::Undefined);
            nestedShould = nestedShould || w.ShouldPush(false, false);
            w.NotePushed(book.Entries());   // a nested call never pushes; noted anyway, it is not counted
            w.EndCall();
            book.Remove(lists);
            w.EndCall();
        }
        const DrawCounts& c = w.Counts();
        const std::string status = w.StatusLine(DetouredRoutes());
        check("draws/target_push_held_once_per_outermost_call",
              c.pushHeld == 2 && c.pushSkipped == 0 && !nestedShould && !w.Pushing() && lists.adds == 24 && lists.deletes == 24
                  && status.find(" push-held=2 push-skipped=0 ") != std::string::npos,
              status);
    }
    {
        // Target: each silent way a call that was to push ends without a Held push is one
        // push-skipped and no push-held - the lists unread, no stand-ins, nothing planned, a
        // list's length unread (None), a push its tails do not show (NotHeld), an add that
        // throws (Threw), a throw the adapter swallows - noted as M1pDrawsCall notes it. A call
        // that may not push (Mark 1 drops on) notes nothing, and a held push is no skip.
        namespace D = ForgePact::Mark1Drops;
        const std::vector<D::PushEntry> plan = { D::PushEntry{ ForgePact::Mark1Set::Piece::Helmet, { 0, 7, 10 }, 1, 2 } };
        Draws w;
        w.SetOn(true, DetouredRoutes());
        w.SetPush(2, false, false);
        DrawPushBook book;
        FakeDrawLists ok, unreadable, hidden, throwing;
        unreadable.unreadable = 2;
        hidden.tailHidden = true;
        throwing.throwAfter = 3;
        int removedLines = 0;
        // M1pDrawsPush's shape: true only when the book read the push back Held.
        auto through = [&](FakeDrawLists& lists, const std::vector<D::PushEntry>& p) {
            return [&w, &book, &removedLines, &lists, p]() {
                std::vector<std::string> lines;
                const DrawPushResult r = book.Push(p, lists, lines);
                removedLines += (int)lines.size();
                if (r != DrawPushResult::Held) return false;
                w.NotePushed(book.Entries());
                return true;
            };
        };
        // M1pDrawsCall's shape.
        auto call = [&](bool mark1On, const std::function<bool()>& push) {
            w.BeginCall(12, DrawArg7::Undefined);
            if (w.ShouldPush(mark1On, false)) {
                bool held = false;
                try { held = push(); } catch (...) { held = false; }
                if (!held) w.NotePushSkipped();
            }
            if (book.Pushed() > 0) book.Remove(ok);
            w.EndCall();
        };
        call(false, [] { return false; });                                   // the lists unread
        call(false, [] { return false; });                                   // no stand-ins resolved
        call(false, through(ok, {}));                                        // nothing planned: None
        call(false, through(unreadable, plan));                              // a length unread: None
        call(false, through(hidden, plan));                                  // not on the tails: NotHeld
        call(false, through(throwing, plan));                                // an add threw: Threw
        call(false, []() -> bool { throw std::runtime_error("adapter"); });  // swallowed
        const uint64_t skippedSilent = w.Counts().pushSkipped;
        const uint64_t heldSilent = w.Counts().pushHeld;
        call(true, through(ok, plan));                                       // may not push: nothing noted
        const uint64_t skippedRefused = w.Counts().pushSkipped;
        call(false, through(ok, plan));                                      // the positive control: held
        // Every list is as found, except the tails the fake hides: the tail rule leaves those as
        // found, one removal line each.
        bool empty = true;
        for (const FakeDrawLists* l : { &ok, &unreadable, &throwing })
            for (const auto& v : l->lists) empty = empty && v.empty();
        const DrawCounts& c = w.Counts();
        check("draws/target_push_skipped_on_every_silent_path",
              skippedSilent == 7 && heldSilent == 0 && skippedRefused == 7 && c.pushSkipped == 7 && c.pushHeld == 1 && empty
                  && hidden.adds == 12 && hidden.deletes == 0 && removedLines == 6 && throwing.adds == 3 && throwing.deletes == 3
                  && unreadable.adds == 0 && ok.adds == 12 && ok.deletes == 12 && c.calls == 9,
              w.StatusLine(DetouredRoutes()) + " removal lines " + std::to_string(removedLines));
    }
    {
        // Target: pushed-reads counts a `draw`-scope repository read of a triple this call
        // pushed (a nested DropItem riding on the entries too); a read of a game triple, of a
        // pushed pair under another class, unreadable or not whole, inside a nested roll, or
        // before or after the call does not.
        namespace D = ForgePact::Mark1Drops;
        const std::vector<D::PushEntry> plan = { D::PushEntry{ ForgePact::Mark1Set::Piece::Helmet, { 0, 7, 10 }, 1, 1 },
                                                 D::PushEntry{ ForgePact::Mark1Set::Piece::Boots, { 2, 9, 31 }, 1, 1 } };
        FakeDrawLists lists;
        DrawPushBook book;
        Draws w;
        w.SetOn(true, DetouredRoutes());
        w.SetPush(1, false, false);
        w.NoteRead(true, 0, 7, 10);                  // before the call: nothing pushed
        w.BeginCall(12, DrawArg7::Undefined);
        std::vector<std::string> lines;
        const bool held = w.ShouldPush(false, false) && book.Push(plan, lists, lines) == DrawPushResult::Held;
        if (held) w.NotePushed(book.Entries());
        w.NoteRead(true, 0, 7, 10);                  // pushed: counts
        w.NoteRead(true, 2, 9, 31);                  // pushed: counts
        w.NoteRead(true, 0, 7, 11);                  // a game triple
        w.NoteRead(true, 1, 7, 10);                  // the pair under another class
        w.NoteRead(false, 0, 7, 10);                 // unread
        w.NoteRead(true, 0, 7, 10.5);                // not whole
        w.EnterRoll(DrawScope::DropUnique);
        w.NoteRead(true, 0, 7, 10);                  // inside a nested roll
        w.LeaveRoll();
        w.BeginCall(12, DrawArg7::Undefined);
        w.NoteRead(true, 2, 9, 31);                  // a nested DropItem's draw: counts
        w.EndCall();
        book.Remove(lists);
        w.EndCall();
        w.NoteRead(true, 0, 7, 10);                  // after the call: nothing pushed
        const DrawCounts& c = w.Counts();
        const std::string status = w.StatusLine(DetouredRoutes());
        check("draws/target_pushed_reads_only_draw_reads_of_a_pushed_triple",
              held && c.pushedReads == 3 && c.reads == 10 && c.pushHeld == 1 && c.pushSkipped == 0
                  && status.find(" push-held=1 push-skipped=0 pushed-reads=3 anomalies=0") != std::string::npos,
              status);
    }

    // ---- pool draw ------------------------------------------------------------------------
    {
        namespace D = ForgePact::Mark1Drops;
        auto e = [](double t, double sub, double b) { D::ListEntry x; x.triple = true; x.t = t; x.sub = sub; x.b = b; return x; };
        // Helmets: 0/7/10 SS once on [0..4] (the best SS: largest count 1, then the lower b),
        // 0/7/11 SS on [5] alone (largest 1, higher b), 0/7/8 SS twice on [0] (largest 2: loses
        // despite the lowest b), 0/7/13 tier 4 (below SS), 0/7/14 rarity 7, 0/7/15 field 40 set,
        // 0/7/16 field 41 set, 0/7/17 unread. Elf: 10/0/5 and 10/0/3, tier 4, the same counts
        // (the lower b wins). Body, boots, gloves: none.
        D::KillLists lists;
        for (int k = 0; k < 5; ++k)
            lists[(size_t)k] = { e(0, 7, 10), e(0, 7, 13), e(0, 7, 14), e(0, 7, 15), e(0, 7, 16), e(0, 7, 17), e(10, 0, 5), e(10, 0, 3) };
        lists[5] = { e(0, 7, 11), e(10, 0, 5), e(10, 0, 3) };
        lists[0].push_back(e(0, 7, 8));
        lists[0].push_back(e(0, 7, 8));
        D::ListEntry junk;
        lists[1].push_back(junk);
        lists[1].push_back(e(7, 1, 1));   // not a class the form reads
        std::map<long long, DrawVerdict> verdicts = {
            { 10, { 6, 5, false, false } }, { 11, { 6, 5, false, false } }, { 8, { 6, 5, false, false } },
            { 13, { 6, 4, false, false } }, { 14, { 7, 5, false, false } }, { 15, { 6, 5, true, false } },
            { 16, { 6, 5, false, true } },  { 5, { 6, 4, false, false } },  { 3, { 6, 4, false, false } },
        };
        int asked = 0;
        bool otherClassAsked = false;
        const std::vector<PoolDrawClass> classes = PoolDraw(lists, [&](const D::Triple& k, DrawVerdict& v) {
            ++asked;
            if (k.t == 7) otherClassAsked = true;
            const auto it = verdicts.find(k.b);
            if (it == verdicts.end()) return false;
            v = it->second;
            return true;
        });
        const std::vector<std::string> lines = PoolDrawLines(classes);
        check("pool/draw_candidate_rule",
              classes.size() == 5 && classes[0].candidates == 4 && classes[0].ss == 3 && classes[0].bestSs && classes[0].bestSs->triple.b == 10
                  && classes[0].bestOther && classes[0].bestOther->triple.b == 13 && classes[4].candidates == 2 && classes[4].ss == 0
                  && !classes[4].bestSs && classes[4].bestOther && classes[4].bestOther->triple.b == 3 && classes[1].candidates == 0
                  && asked == 10 && !otherClassAsked,
              std::to_string(asked) + " asked");
        check("pool/draw_lines_exact",
              lines.size() == 5
                  && lines[0] == "mark1probe pool draw: helmet class=0 candidates=4 ss=3 best-ss=7:10@6/5x1,1,1,1,1,0 best-other=7:13@6/4x1,1,1,1,1,0"
                  && lines[1] == "mark1probe pool draw: body class=1 candidates=0 ss=0 best-ss=none best-other=none"
                  && lines[2] == "mark1probe pool draw: boots class=2 candidates=0 ss=0 best-ss=none best-other=none"
                  && lines[3] == "mark1probe pool draw: gloves class=4 candidates=0 ss=0 best-ss=none best-other=none"
                  && lines[4] == "mark1probe pool draw: elf class=10 candidates=2 ss=0 best-ss=none best-other=0:3@6/4x1,1,1,1,1,1",
              lines.size() == 5 ? lines[0] + " | " + lines[4] : std::string("not five lines"));
        const std::vector<std::pair<std::string, int>> kinds = PoolDrawClasses();
        check("pool/draw_classes_are_the_pieces_and_pocket_elf",
              kinds.size() == 5 && kinds[0] == std::make_pair(std::string("helmet"), 0) && kinds[1] == std::make_pair(std::string("body"), 1)
                  && kinds[2] == std::make_pair(std::string("boots"), 2) && kinds[3] == std::make_pair(std::string("gloves"), 4)
                  && kinds[4] == std::make_pair(std::string("elf"), 10));

        // The lever's stand-ins are these candidates: the helmet's SS best (0/7/10), not the
        // field-41 or rarity-7 entries `pool kill`'s rule could take; body, boots and gloves have
        // none and are refused. With no SS candidate the below-SS best is the fallback.
        const DrawLeverStandIns picks = DrawLeverPick(classes);
        std::vector<PoolDrawClass> belowOnly = classes;
        belowOnly[0].bestSs.reset();
        const DrawLeverStandIns fallback = DrawLeverPick(belowOnly);
        check("draws/target_lever_stand_ins_are_pool_draws_candidates_ss_first",
              picks[0].pick && picks[0].pick->triple.t == 0 && picks[0].pick->triple.sub == 7 && picks[0].pick->triple.b == 10
                  && picks[0].ss && !picks[0].fallback && !picks[1].pick && !picks[2].pick && !picks[3].pick
                  && fallback[0].pick && fallback[0].pick->triple.b == 13 && !fallback[0].ss && fallback[0].fallback);
        const std::vector<D::PushEntry> plan = DrawPushPlan(picks, 3);
        DrawLeverStandIns sentBack = picks;
        sentBack[0].pick->f41 = true;
        DrawLeverStandIns otherRarity = picks;
        otherRarity[0].pick->rarity = 7;
        check("draws/target_lever_plan_pushes_only_what_the_draw_keeps",
              plan.size() == 1 && plan[0].piece == ForgePact::Mark1Set::Piece::Helmet && plan[0].triple.b == 10 && plan[0].n == 1
                  && plan[0].copies == 3 && DrawPushPlan(picks, 0).empty() && DrawPushPlan(picks, 900)[0].copies == 400
                  && DrawPushPlan(sentBack, 3).empty() && DrawPushPlan(otherRarity, 3).empty()
                  && DrawLeverRefusal(*picks[0].pick).empty() && DrawLeverRefusal(*sentBack[0].pick) == "field 41 set"
                  && DrawLeverRefusal(*otherRarity[0].pick) == "rarity 7"
                  && DrawLeverStandInText(sentBack[0]) == "helmet=refused (7:10: field 41 set)");
        const std::string none = "=refused (class %d has no draw candidate: no rarity-6 entry with fields 40 and 41 clear on lootListUnique[0..5])";
        auto refusedText = [&](const char* key, int t) {
            std::string s = std::string(key) + none;
            return s.replace(s.find("%d"), 2, std::to_string(t));
        };
        const std::string head = "mark1probe draws push: stand-ins (pool draw's candidates, SS first, the side Mark 1's kill draw is to use) ";
        const std::string rest = " | " + refusedText("body", 1) + " | " + refusedText("boots", 2) + " | " + refusedText("gloves", 4);
        const std::string onLine = DrawLeverLine(picks, true);
        check("draws/target_lever_line_exact",
              onLine == head + "helmet=7:10 rarity=6 tier=5 f40=0 f41=0 lists=1,1,1,1,1,0 side=ss" + rest
                  && DrawLeverLine(picks, false) == onLine + " (the watch is off: `draws on` to push)"
                  && DrawLeverLine(fallback, true)
                         == head + "helmet=7:13 rarity=6 tier=4 f40=0 f41=0 lists=1,1,1,1,1,0 side=below-ss (fallback)" + rest,
              onLine);
        // Pushed and taken off exactly, through the same book the watch uses.
        FakeDrawLists fake;
        for (int k = 0; k < 6; ++k) fake.lists[(size_t)k] = { { 0, 7, 10 }, { 0, 7, 13 } };
        DrawPushBook book;
        std::vector<std::string> pushLines;
        const DrawPushResult pushed = book.Push(plan, fake, pushLines);
        bool grown = true;
        for (const auto& l : fake.lists) grown = grown && l.size() == 5 && l.back() == D::Triple{ 0, 7, 10 };
        const std::vector<std::string> removed = book.Remove(fake);
        bool back = true;
        for (const auto& l : fake.lists) back = back && l.size() == 2;
        check("draws/target_lever_pushes_the_draw_stand_in_onto_each_list_and_off",
              pushed == DrawPushResult::Held && grown && fake.adds == 18 && removed.empty() && pushLines.empty() && back
                  && fake.deletes == 18 && book.Pushed() == 0,
              std::to_string(fake.adds) + " adds, " + std::to_string(fake.deletes) + " deletes");
        // Resolved again when any list changed: another array or another length.
        DrawLeverCache cache;
        D::KillListKeys keys{};
        for (int k = 0; k < 6; ++k) keys[(size_t)k] = D::ListKey{ (uint64_t)(100 + k), 9 };
        const bool firstStale = cache.Stale(keys) && cache.Find() == nullptr;
        cache.Store(keys, picks);
        const bool held = !cache.Stale(keys) && cache.Find() && (*cache.Find())[0].pick && (*cache.Find())[0].pick->triple.b == 10;
        D::KillListKeys longer = keys;
        longer[3].length = 10;
        D::KillListKeys other = keys;
        other[5].id = 999;
        check("draws/target_lever_stand_ins_resolved_again_when_a_list_changed",
              firstStale && held && cache.Stale(longer) && cache.Stale(other));

        // Baseline: a push with no side picks exactly as built (kDrawLeverPrefersSs: SS first,
        // the below-SS best a named fallback), with the same line, plan and reply.
        const DrawLeverStandIns plain = DrawLeverPick(classes, DrawSide::SsFirst);
        const DrawLeverStandIns plainFallback = DrawLeverPick(belowOnly, DrawSide::SsFirst);
        bool same = true;
        for (size_t i = 0; i < picks.size(); ++i)
            same = same && DrawLeverStandInText(plain[i]) == DrawLeverStandInText(picks[i])
                   && DrawLeverStandInText(plainFallback[i]) == DrawLeverStandInText(fallback[i]);
        const Command noSide = parsed("draws push 25");
        Draws sideless;
        const std::string plainReply = sideless.SetPush(25, false, false);
        const std::vector<D::PushEntry> plainPlan = DrawPushPlan(plain, 3);
        check("draws/baseline_push_without_a_side_keeps_ss_first",
              kDrawLeverPrefersSs && same && DrawLeverLine(plain, true) == onLine && DrawLeverLine(plain, true, DrawSide::SsFirst) == onLine
                  && plainPlan.size() == 1 && plainPlan[0].triple.b == 10 && plainFallback[0].fallback && plainFallback[0].pick->triple.b == 13
                  && noSide.form == Form::DrawsPush && noSide.count == 25 && noSide.side == DrawSide::SsFirst
                  && sideless.Side() == DrawSide::SsFirst
                  && plainReply == "mark1probe draws push: c=25 - while the watch is on, each DropItem call pushes 25 copies of each Mark 1 kill "
                                   "stand-in onto each of lootListUnique[0..5]; nothing is rewritten"
                  && sideless.StatusLine(DrawRoutes{}).find(" push=25 push-side=ss-first picks=0 ") != std::string::npos,
              plainReply);

        // Target: `below` takes only below-SS candidates and `ss` only SS ones, with no
        // fallback: a class with none on the asked side pushes nothing and is named as refused,
        // and every stand-in still names its side.
        const DrawLeverStandIns below = DrawLeverPick(classes, DrawSide::Below);
        const DrawLeverStandIns ssOnly = DrawLeverPick(classes, DrawSide::Ss);
        std::vector<PoolDrawClass> ssOnlyClasses = classes;
        ssOnlyClasses[0].bestOther.reset();
        const DrawLeverStandIns belowNone = DrawLeverPick(ssOnlyClasses, DrawSide::Below);
        const DrawLeverStandIns ssNone = DrawLeverPick(belowOnly, DrawSide::Ss);
        const std::vector<D::PushEntry> belowPlan = DrawPushPlan(below, 25);
        auto sideRefused = [](const char* key, int t, const char* what, const char* side) {
            return std::string(key) + "=refused (class " + std::to_string(t) + " has no " + what
                + " draw candidate on lootListUnique[0..5]; side=" + side + " takes no fallback)";
        };
        const std::string belowRest = " | " + sideRefused("body", 1, "below-SS", "below") + " | " + sideRefused("boots", 2, "below-SS", "below")
            + " | " + sideRefused("gloves", 4, "below-SS", "below");
        const std::string belowLine = DrawLeverLine(below, true, DrawSide::Below);
        const std::string ssLine = DrawLeverLine(ssOnly, false, DrawSide::Ss);
        check("draws/target_push_below_picks_only_below_ss_and_refuses_a_class_without_one",
              below[0].pick && below[0].pick->triple.b == 13 && below[0].pick->tier == 4 && !below[0].ss && !below[0].fallback
                  && ssOnly[0].pick && ssOnly[0].pick->triple.b == 10 && ssOnly[0].ss && !ssOnly[0].fallback
                  && !belowNone[0].pick && DrawPushPlan(belowNone, 25).empty() && !ssNone[0].pick && DrawPushPlan(ssNone, 25).empty()
                  && belowPlan.size() == 1 && belowPlan[0].triple.b == 13 && belowPlan[0].copies == 25
                  && belowLine == "mark1probe draws push: stand-ins (pool draw's candidates, below SS only, side=below, no fallback) "
                                  "helmet=7:13 rarity=6 tier=4 f40=0 f41=0 lists=1,1,1,1,1,0 side=below-ss" + belowRest
                  && ssLine.rfind("mark1probe draws push: stand-ins (pool draw's candidates, SS only, side=ss, no fallback) "
                                  "helmet=7:10 rarity=6 tier=5 f40=0 f41=0 lists=1,1,1,1,1,0 side=ss | ", 0) == 0
                  && DrawLeverStandInText(belowNone[0]) == sideRefused("helmet", 0, "below-SS", "below")
                  && DrawLeverStandInText(ssNone[0]) == sideRefused("helmet", 0, "SS", "ss"),
              belowLine);
        // The side word: parsed, refused with its reason, named in the reply and on the status
        // line (`-` while the push is off), and a reason to resolve the stand-ins again.
        const Command pushBelow = parsed("draws push 25 below");
        const Command pushSs = parsed("DRAWS PUSH 25 SS");
        Draws sided;
        const std::string belowReply = sided.SetPush(25, false, false, DrawSide::Below);
        const std::string sidedStatus = sided.StatusLine(DrawRoutes{});
        const std::string ssReply = sided.SetPush(25, false, false, DrawSide::Ss);
        const std::string offReply = sided.SetPush(0, false, false, DrawSide::Below);
        DrawLeverCache sideCache;
        sideCache.Store(keys, below, DrawSide::Below);
        const std::string base = "mark1probe draws push: c=25 - while the watch is on, each DropItem call pushes 25 copies of each Mark 1 "
                                 "kill stand-in onto each of lootListUnique[0..5]; nothing is rewritten";
        check("draws/target_push_side_parse_reply_and_status",
              pushBelow.form == Form::DrawsPush && pushBelow.count == 25 && pushBelow.side == DrawSide::Below
                  && pushSs.form == Form::DrawsPush && pushSs.count == 25 && pushSs.side == DrawSide::Ss
                  && why("draws push 25 left") == "usage: draws push <c> [ss|below]|off, c a whole number 1..400, the side ss or below, not left"
                  && why("draws push off below") == "usage: draws push <c> [ss|below]|off, c a whole number 1..400, not off"
                  && belowReply == base + "; side=below: below-SS stand-ins only, no fallback"
                  && ssReply == base + "; side=ss: SS stand-ins only, no fallback"
                  && sidedStatus.find(" push=25 push-side=below picks=0 ") != std::string::npos
                  && offReply == "mark1probe draws push: off - nothing is pushed" && sided.Side() == DrawSide::SsFirst
                  && sided.StatusLine(DrawRoutes{}).find(" push=off push-side=- picks=0 ") != std::string::npos
                  && !sideCache.Stale(keys, DrawSide::Below) && sideCache.Stale(keys, DrawSide::Ss) && sideCache.Stale(keys),
              belowReply + " | " + sidedStatus);
    }

    // ---- walkable -------------------------------------------------------------
    {
        WalkPoint w;
        w.x = 320;
        w.y = 96.5;
        w.roomIn = true;
        w.grid = Grid::Free;
        w.family = Family::Free;
        w.wallLine = WallLine::Clear;
        w.tilesRead = true;
        w.tiles = { { "Tiles_1", 0 }, { "Tiles_2", 17 } };
        check("walkable/line_with_every_field_free", WalkLine(w)
              == "walkable x=320 y=96.5 room=in grid=free family=free wall-line=clear tile=Tiles_1:0,Tiles_2:17"
                 " grid-raw=none family-raw=none wall-line-raw=none", WalkLine(w));
        w.roomIn = false;
        w.grid = Grid::Blocked;
        w.family = Family::Blocked;
        w.wallLine = WallLine::Crossed;
        w.tiles = {};
        check("walkable/line_with_every_field_blocked", WalkLine(w)
              == "walkable x=320 y=96.5 room=out grid=blocked family=blocked wall-line=crossed tile=none"
                 " grid-raw=none family-raw=none wall-line-raw=none met=unreadable", WalkLine(w));
        w.grid = Grid::Unreadable;
        w.tiles = { { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 }, { "e", 5 } };
        check("walkable/grid_unreadable_and_four_tile_layers_at_most", WalkLine(w)
              == "walkable x=320 y=96.5 room=out grid=unreadable family=blocked wall-line=crossed tile=a:1,b:2,c:3,d:4"
                 " grid-raw=none family-raw=none wall-line-raw=none met=unreadable", WalkLine(w));
        // A layer found whose cell could not be read prints unreadable, not a 0.
        w.tiles = { { "Tiles_1", 3 }, { "Tiles_2", std::nullopt } };
        check("walkable/a_tile_cell_with_no_value_prints_unreadable", WalkLine(w)
              == "walkable x=320 y=96.5 room=out grid=unreadable family=blocked wall-line=crossed tile=Tiles_1:3,Tiles_2:unreadable"
                 " grid-raw=none family-raw=none wall-line-raw=none met=unreadable",
              WalkLine(w));
    }
    {
        // Nothing read yet: every reading says unreadable. A failed
        // place_meeting or collision_line must never print as open floor.
        const WalkPoint none{};
        check("walkable/a_point_with_nothing_read_is_unreadable_throughout", WalkLine(none)
              == "walkable x=0 y=0 room=out grid=unreadable family=unreadable wall-line=unreadable tile=unreadable"
                 " grid-raw=none family-raw=none wall-line-raw=none", WalkLine(none));
        WalkPoint tilesOnly;
        tilesOnly.tiles = { { "Tiles_1", 5 } };
        check("walkable/tiles_not_enumerated_print_unreadable", WalkLine(tilesOnly)
              == "walkable x=0 y=0 room=out grid=unreadable family=unreadable wall-line=unreadable tile=unreadable"
                 " grid-raw=none family-raw=none wall-line-raw=none",
              WalkLine(tilesOnly));
    }
    {
        // place_meeting answers whether the mask meets the family: a bool, or
        // a number where 0 means no meeting. Live 2 read family=blocked at the
        // player's own position; a numeric 0 read as "id >= 0" is one cause.
        const auto raw = [](const char* kind, double v) {
            RawResult r;
            r.state = RawResult::State::Read;
            r.kind = kind;
            r.number = v;
            return r;
        };
        const bool isFree = FamilyFrom(raw("bool", 0)) == Family::Free && FamilyFrom(raw("real", 0)) == Family::Free
            && FamilyFrom(raw("int32", 0)) == Family::Free && FamilyFrom(raw("int64", 0)) == Family::Free;
        const bool isBlocked = FamilyFrom(raw("bool", 1)) == Family::Blocked && FamilyFrom(raw("real", 1)) == Family::Blocked
            && FamilyFrom(raw("int64", 1)) == Family::Blocked;
        check("walkable/meeting_false_or_zero_is_free_true_or_one_blocked", isFree && isBlocked);
        // Negative control: anything else is no answer at all.
        RawResult kindless = raw("undefined", 0);
        kindless.number.reset();
        RawResult failed;
        failed.state = RawResult::State::Failed;
        const bool unread = FamilyFrom(raw("string", 0)) == Family::Unreadable && FamilyFrom(raw("ref", 0)) == Family::Unreadable
            && FamilyFrom(kindless) == Family::Unreadable && FamilyFrom(failed) == Family::Unreadable
            && FamilyFrom(RawResult{}) == Family::Unreadable;
        check("walkable/meeting_of_an_unrecognised_kind_is_unreadable", unread);

        // collision_line answers an instance or noone (-4): never "0 = clear".
        const bool crossed = WallLineFrom(raw("ref", 100234)) == WallLine::Crossed
            && WallLineFrom(raw("real", 100234)) == WallLine::Crossed && WallLineFrom(raw("int32", 0)) == WallLine::Crossed
            && WallLineFrom(raw("bool", 1)) == WallLine::Crossed;
        const bool clear = WallLineFrom(raw("real", -4)) == WallLine::Clear && WallLineFrom(raw("ref", -4)) == WallLine::Clear
            && WallLineFrom(raw("bool", 0)) == WallLine::Clear;
        const bool lineUnread = WallLineFrom(raw("string", 5)) == WallLine::Unreadable && WallLineFrom(kindless) == WallLine::Unreadable
            && WallLineFrom(failed) == WallLine::Unreadable;
        check("walkable/wall_line_instance_crossed_noone_clear", crossed && clear && lineUnread);

        // mp_grid_get_cell: -1 blocked, 0 free, anything not a number unreadable.
        const bool grid = GridFrom(raw("real", -1)) == Grid::Blocked && GridFrom(raw("real", 0)) == Grid::Free
            && GridFrom(raw("int32", 0)) == Grid::Free && GridFrom(raw("bool", 0)) == Grid::Unreadable
            && GridFrom(failed) == Grid::Unreadable;
        check("walkable/grid_cell_minus_one_blocked", grid);

        // Each builtin-read field prints the raw answer it came from.
        WalkPoint p;
        p.x = 100;
        p.y = 200;
        p.roomIn = true;
        p.gridRaw = raw("real", 0);
        p.familyRaw = raw("bool", 0);
        p.wallLineRaw = raw("real", -4);
        p.grid = GridFrom(p.gridRaw);
        p.family = FamilyFrom(p.familyRaw);
        p.wallLine = WallLineFrom(p.wallLineRaw);
        p.tilesRead = true;
        check("walkable/raw_fields_follow_the_line", WalkLine(p)
              == "walkable x=100 y=200 room=in grid=free family=free wall-line=clear tile=none"
                 " grid-raw=real:0 family-raw=bool:false wall-line-raw=real:-4", WalkLine(p));
        p.familyRaw = raw("real", 1);
        p.family = FamilyFrom(p.familyRaw);
        p.wallLineRaw = failed;
        p.wallLine = WallLineFrom(p.wallLineRaw);
        p.met = MetText(raw("ref", 100234), "Fence_obj");
        check("walkable/raw_error_and_what_a_blocked_family_met", WalkLine(p)
              == "walkable x=100 y=200 room=in grid=free family=blocked wall-line=unreadable tile=none"
                 " grid-raw=real:0 family-raw=real:1 wall-line-raw=error met=Fence_obj:100234", WalkLine(p));
        const bool met = MetText(raw("real", -4), "") == "noone" && MetText(failed, "x") == "error"
            && MetText(RawResult{}, "x") == "none" && MetText(raw("ref", 7), "") == "?:7"
            && MetText(raw("real", 7), "Bad name,x:y") == "Bad_name_x_y:7" && MetText(kindless, "x") == "undefined:?";
        check("walkable/met_names_object_and_id", met);
    }
    {
        const bool room = RoomContains(0, 0, 640, 480) && RoomContains(639.5, 479, 640, 480) && !RoomContains(640, 10, 640, 480)
            && !RoomContains(-1, 10, 640, 480) && !RoomContains(10, 480, 640, 480);
        const bool cells = GridCellOf(0) == 0 && GridCellOf(15.9) == 0 && GridCellOf(16) == 1 && GridCellOf(-1) == -1;
        check("walkable/room_and_grid_cell", room && cells);
    }
    {
        // GameMaker angles: 0 is right, 90 is up (y falls).
        const auto right = ScanPoints(100, 100, 0, 32, 3);
        const auto up = ScanPoints(100, 100, 90, 16, 2);
        const auto diag = ScanPoints(0, 0, 225, 10, 1);
        const bool r = right.size() == 3 && right[0] == std::make_pair(132.0, 100.0) && right[2] == std::make_pair(196.0, 100.0);
        const bool u = up.size() == 2 && up[0] == std::make_pair(100.0, 84.0) && up[1] == std::make_pair(100.0, 68.0);
        const bool d = diag.size() == 1 && diag[0] == std::make_pair(-7.071, 7.071);
        check("walkable/scan_point_list", r && u && d);
    }

    // ---- parsers --------------------------------------------------------------
    {
        const bool bare = parsed("").form == Form::Status;
        const bool simple = parsed("hook").form == Form::Hook && parsed("off").form == Form::Off && parsed("show").form == Form::Show
            && parsed("show reset").form == Form::ShowReset && parsed("HOOK").form == Form::Hook;
        const Command args = parsed("args 20");
        check("parse/bare_hook_off_show_args", bare && simple && args.form == Form::Args && args.count == 20);
    }
    {
        const Command w1 = parsed("watch pst 120");
        const Command w2 = parsed("watch queue 60 after TalentUse");
        const bool all = parsed("watch player 1").scope == Scope::Player && parsed("watch profile 3600").scope == Scope::Profile
            && parsed("watch input 10").scope == Scope::Input;
        check("parse/watch_forms", w1.form == Form::Watch && w1.scope == Scope::Pst && w1.frames == 120 && !w1.after
              && w2.scope == Scope::Queue && w2.after && w2.afterRow == Row::TalentUse && all);
    }
    {
        const Command s = parsed("set profile.inputState[30] 1 during CheckTalentUse");
        const Command t = parsed("set input[7] -2.5 during TalentUse 90");
        const Command b = parsed("set player.autoAim false during CheckTalentUse");
        const bool set = s.form == Form::Set && s.lever.kind == LeverKind::Set && s.lever.target.scope == Scope::Profile
            && s.lever.target.name == "inputState" && s.lever.target.indexed && s.lever.target.index == 30
            && s.lever.value.number == 1 && !s.lever.value.isBool && s.lever.row == Row::CheckTalentUse && s.lever.frames == 600;
        const bool input = t.lever.target.scope == Scope::Input && t.lever.target.indexed && t.lever.target.index == 7
            && t.lever.value.number == -2.5 && t.lever.frames == 90 && TargetText(t.lever.target) == "input[7]";
        const bool boolean = b.lever.value.isBool && b.lever.value.number == 0 && !b.lever.target.indexed;
        const Command sc = parsed("scale pst.spd 0.5 during PlayerMovement 3600");
        const bool scale = sc.form == Form::Scale && sc.lever.kind == LeverKind::Scale && sc.lever.mult == 0.5
            && sc.lever.frames == 3600 && sc.lever.target.scope == Scope::Pst;
        check("parse/set_and_scale_forms", set && input && boolean && scale);
    }
    {
        const Command m = parsed("move PlayerMovement 0.5");
        const Command m3 = parsed("move skillsLeap 3 120");
        const Command st = parsed("stat 66 3");
        const bool ok = m.form == Form::Move && m.lever.row == Row::PlayerMovement && m.lever.mult == 0.5 && m.lever.frames == 600
            && m3.lever.row == Row::SkillsLeap && m3.lever.mult == 3 && m3.lever.frames == 120
            && st.form == Form::Stat && st.lever.statId == 66 && st.lever.mult == 3;
        check("parse/move_and_stat_forms", ok);
    }
    {
        const Command c = parsed("count Wall_Parent_obj 300");
        const Command p = parsed("pos 600");
        const Command h = parsed("walkable here 48 -16");
        const Command a = parsed("walkable at 1024.5 768");
        const Command s = parsed("walkable scan 45 32 8");
        const bool ok = c.form == Form::Count && c.object == "Wall_Parent_obj" && c.frames == 300
            && p.form == Form::Pos && p.frames == 600
            && h.form == Form::WalkHere && h.a == 48 && h.b == -16
            && a.form == Form::WalkAt && a.a == 1024.5 && a.b == 768
            && s.form == Form::WalkScan && s.a == 45 && s.b == 32 && s.count == 8;
        check("parse/count_pos_and_walkable_forms", ok);
    }
    {
        const Command sl = parsed("statlog 64 120");
        const Command sp = parsed("statpin 64");
        const Command sp2 = parsed("statpin 66 150");
        const Command h = parsed("hold player.hSpeedFactor x0.5 during PlayerMovement 240");
        const Command hv = parsed("hold effect[182] 0 during PlayerMovement 150");
        const Command hb = parsed("hold builtin.image_speed false during TalentUse");
        const Command b = parsed("base 4 2");
        const Command w = parsed("watch effect 90 after TalentUse");
        const Command wb = parsed("watch builtin 90");
        const Command scan = parsed("walkable scan 90 96 60");
        const bool ok = sl.form == Form::Statlog && sl.statId == 64 && sl.frames == 120
            && sp.form == Form::Statpin && sp.lever.kind == LeverKind::Pin && sp.lever.statId == 64 && sp.lever.frames == 600
            && sp.lever.row == Row::ReturnSpecificStat && sp2.lever.statId == 66 && sp2.lever.frames == 150
            && h.form == Form::Hold && h.lever.kind == LeverKind::Hold && h.lever.holdMult && h.lever.mult == 0.5
            && h.lever.target.scope == Scope::Player && h.lever.target.name == "hSpeedFactor" && h.lever.row == Row::PlayerMovement
            && h.lever.frames == 240
            && !hv.lever.holdMult && hv.lever.value.number == 0 && hv.lever.target.scope == Scope::Effect
            && hv.lever.target.name == "playerEffect" && hv.lever.target.index == 182 && hv.lever.frames == 150
            && hb.lever.target.scope == Scope::Builtin && hb.lever.value.isBool
            && b.form == Form::Base && b.itemClass == 4 && b.base == 2
            && w.form == Form::Watch && w.scope == Scope::Effect && w.after && wb.scope == Scope::Builtin
            && scan.form == Form::WalkScan && scan.count == 60
            && parsed("move place_meeting 0.5 240").lever.row == Row::PlaceMeeting
            && parsed("move MP_LINEAR_STEP 0.5").lever.row == Row::MpLinearStep;
        check("parse/phase2_forms", ok);
    }
    {
        const std::vector<std::pair<std::string, std::string>> refused = {
            { "bogus", "unknown form bogus (hook, off, show, args, watch, set, scale, hold, move, stat, statpin, statlog, count, pos, base, spawn, heroic, unique, pool, copies, killgate, draws or walkable)" },
            { "heroic", "usage: heroic <n>, n a whole number 1..20" },
            { "heroic 0", "usage: heroic <n>, n a whole number 1..20, not 0" },
            { "heroic 21", "usage: heroic <n>, n a whole number 1..20, not 21" },
            { "heroic 5 5", "usage: heroic <n>, n a whole number 1..20" },
            { "unique", "usage: unique <n>, n a whole number 1..20" },
            { "unique 0", "usage: unique <n>, n a whole number 1..20, not 0" },
            { "unique 21", "usage: unique <n>, n a whole number 1..20, not 21" },
            { "pool 6", "usage: pool [<k>|kill|draw], k a sub-list 0..5, not 6" },
            { "pool x", "usage: pool [<k>|kill|draw], k a sub-list 0..5, not x" },
            { "pool 1 2", "usage: pool [<k>|kill|draw], k a sub-list 0..5" },
            { "pool kill 2", "usage: pool [<k>|kill|draw], k a sub-list 0..5" },
            { "copies", "usage: copies <k>, k a whole number 1..400" },
            { "copies 0", "usage: copies <k>, k a whole number 1..400, not 0" },
            { "copies 401", "usage: copies <k>, k a whole number 1..400, not 401" },
            { "killgate 2", "usage: killgate [0|1], not 2" },
            { "killgate on", "usage: killgate [0|1], not on" },
            { "killgate 1 0", "usage: killgate [0|1]" },
            { "statlog 64", "usage: statlog <id> <frames>" },
            { "statlog 1000 60", "stat id must be a whole number 0..999, not 1000" },
            { "statlog 64 0", "frames must be a whole number 1..3600, not 0" },
            { "statpin", "usage: statpin <id> [frames]" },
            { "statpin 64 150 9", "usage: statpin <id> [frames]" },
            { "statpin x", "stat id must be a whole number 0..999, not x" },
            { "statpin 64 3601", "frames must be a whole number 1..3600, not 3601" },
            { "hold player.x 1", "usage: hold <scope>.<name>[<i>] <value|x<mult>> during <row> [frames]" },
            { "hold player.x y2 during TalentUse", "a hold value is a number, true, false or x<mult>, not y2" },
            { "hold player.x x during TalentUse", "a hold value is a number, true, false or x<mult>, not x" },
            { "hold player.x x101 during TalentUse", "mult must be a number -100..100, not 101" },
            { "hold effect.x 1 during TalentUse", "a target is <scope>.<name>[<i>], input[<i>], effect[<i>] or pst[<i>], not effect.x" },
            { "hold effect[x] 1 during TalentUse", "an index is a whole number 0..9999, not effect[x]" },
            { "base 0", "usage: base <class 0-19> <b>" },
            { "base 20 0", "class must be a whole number 0..19, not 20" },
            { "base 0 10000", "b must be a whole number 0..9999, not 10000" },
            { "base 0 -1", "b must be a whole number 0..9999, not -1" },
            { "spawn", "usage: spawn <helmet|body|boots|gloves|set>" },
            { "spawn helmet boots", "usage: spawn <helmet|body|boots|gloves|set>" },
            { "spawn ring", "usage: spawn <helmet|body|boots|gloves|set>, not ring" },
            { "hook now", "usage: mark1probe hook" },
            { "off 1", "usage: mark1probe off" },
            { "show all", "usage: mark1probe show [reset]" },
            { "args", "args needs a whole number 1..100" },
            { "args 0", "args needs a whole number 1..100, not 0" },
            { "args 101", "args needs a whole number 1..100, not 101" },
            { "watch pst", "usage: watch <scope> <frames> [after <row>]" },
            { "watch body 10", "unknown scope body (player, pst, profile, input, queue, effect or builtin)" },
            { "watch pst 0", "frames must be a whole number 1..3600, not 0" },
            { "watch pst 3601", "frames must be a whole number 1..3600, not 3601" },
            { "watch pst x", "frames must be a whole number 1..3600, not x" },
            { "watch pst 10 after Bogus", "unknown row Bogus (CheckTalentUse, TalentUse, TalentUseSetSpeed, PlayerMovement, ReturnSpecificStat, StatMovementSpeed, skillsLeap, playerJumpGravity, StatJumpPower, place_meeting, mp_linear_step, mp_linear_step_object, mp_potential_step, mp_potential_step_object, move_towards_point, motion_set, motion_add, move_contact_solid, move_outside_solid or move_and_collide)" },
            { "watch pst 10 before TalentUse", "usage: watch <scope> <frames> [after <row>]" },
            { "set player.x 1", "usage: set <scope>.<name>[<i>] <value> during <row> [frames]" },
            { "set body.x 1 during TalentUse", "unknown scope body (player, pst, profile, input, queue, effect or builtin)" },
            { "set player 1 during TalentUse", "a target is <scope>.<name>[<i>], input[<i>], effect[<i>] or pst[<i>], not player" },
            { "set player.a-b 1 during TalentUse", "a member name is 1..64 letters, digits or _, not a-b" },
            { "set player.x[ 1 during TalentUse", "an index is a whole number 0..9999, not x[" },
            { "set player.x[-1] 1 during TalentUse", "an index is a whole number 0..9999, not x[-1]" },
            { "set input.inputState[3] 1 during TalentUse", "a target is <scope>.<name>[<i>], input[<i>], effect[<i>] or pst[<i>], not input.inputState[3]" },
            { "set input 1 during TalentUse", "a target is <scope>.<name>[<i>], input[<i>], effect[<i>] or pst[<i>], not input" },
            { "set player.x abc during TalentUse", "a value is a number, true or false, not abc" },
            { "set player.x inf during TalentUse", "a value is a number, true or false, not inf" },
            { "set player.x 1e9 during TalentUse", "a value is a number, true or false, not 1e9" },
            { "set player.x 1 during Nope", "unknown row Nope (CheckTalentUse, TalentUse, TalentUseSetSpeed, PlayerMovement, ReturnSpecificStat, StatMovementSpeed, skillsLeap, playerJumpGravity, StatJumpPower, place_meeting, mp_linear_step, mp_linear_step_object, mp_potential_step, mp_potential_step_object, move_towards_point, motion_set, motion_add, move_contact_solid, move_outside_solid or move_and_collide)" },
            { "set player.x 1 while TalentUse", "usage: set <scope>.<name>[<i>] <value> during <row> [frames]" },
            { "set player.x 1 during TalentUse 4000", "frames must be a whole number 1..3600, not 4000" },
            { "set player.x 1 during TalentUse 10 20", "usage: set <scope>.<name>[<i>] <value> during <row> [frames]" },
            { "scale player.x true during TalentUse", "mult must be a number -100..100, not true" },
            { "scale player.x 101 during TalentUse", "mult must be a number -100..100, not 101" },
            { "move PlayerMovement", "usage: move <row> <mult> [frames]" },
            { "move Nope 2", "unknown row Nope (CheckTalentUse, TalentUse, TalentUseSetSpeed, PlayerMovement, ReturnSpecificStat, StatMovementSpeed, skillsLeap, playerJumpGravity, StatJumpPower, place_meeting, mp_linear_step, mp_linear_step_object, mp_potential_step, mp_potential_step_object, move_towards_point, motion_set, motion_add, move_contact_solid, move_outside_solid or move_and_collide)" },
            { "move PlayerMovement x", "mult must be a number -100..100, not x" },
            { "move PlayerMovement 2 0", "frames must be a whole number 1..3600, not 0" },
            { "stat 64", "usage: stat <id> <mult> [frames]" },
            { "stat 1000 2", "stat id must be a whole number 0..999, not 1000" },
            { "stat 6.4 2", "stat id must be a whole number 0..999, not 6.4" },
            { "stat 64 nan", "mult must be a number -100..100, not nan" },
            { "stat 64 2 3601", "frames must be a whole number 1..3600, not 3601" },
            { "count Wall_Parent_obj", "usage: count <object name> <frames>" },
            { "count Wall-obj 10", "an object name is 1..64 letters, digits or _, not Wall-obj" },
            { "count Wall_Parent_obj 9999", "frames must be a whole number 1..3600, not 9999" },
            { "pos", "usage: pos <frames>" },
            { "pos 0", "frames must be a whole number 1..3600, not 0" },
            { "walkable", "usage: walkable here <dx> <dy> | walkable at <x> <y> | walkable scan <deg> <step> <count>" },
            { "walkable there 1 2", "usage: walkable here <dx> <dy> | walkable at <x> <y> | walkable scan <deg> <step> <count>" },
            { "walkable here 1", "usage: walkable here <dx> <dy> | walkable at <x> <y> | walkable scan <deg> <step> <count>" },
            { "walkable here x 2", "walkable needs numbers, not x" },
            { "walkable at 1 2 3", "usage: walkable here <dx> <dy> | walkable at <x> <y> | walkable scan <deg> <step> <count>" },
            { "walkable scan 0 0 4", "scan step must be a number 1..1024, not 0" },
            { "walkable scan 0 16 0", "scan count must be a whole number 1..64, not 0" },
            { "walkable scan 0 16 65", "scan count must be a whole number 1..64, not 65" },
        };
        bool ok = true;
        std::string bad;
        for (const auto& [args, reason] : refused) {
            const std::string got = why(args);
            if (got != reason) { ok = false; bad += "[" + args + "] -> " + got + "; "; }
        }
        check("parse/refused_forms_name_their_reason", ok, bad);
    }
    {
        // The command word itself is the adapter's; every form's usage is listed.
        const std::vector<std::string> usage = Usage();
        bool every = true;
        for (const char* f : { "hook", "off", "show", "args", "watch", "set", "scale", "hold", "move", "stat", "statpin", "statlog",
                               "count", "pos", "base", "spawn", "heroic", "unique", "pool", "pool kill", "copies", "killgate", "draws", "draws push", "pool draw", "walkable" }) {
            bool found = false;
            for (const std::string& l : usage) found = found || l.find(std::string("mark1probe ") + f) != std::string::npos;
            every = every && found;
        }
        check("parse/usage_lists_every_form", every);
        const bool numbers = NumberText(3) == "3" && NumberText(-0.5) == "-0.5" && NumberText(std::nan("")) == "?"
            && ValueText(Value{ true, 1 }) == "true" && ValueText(Value{ true, 0 }) == "false" && ValueText(Num(2.25)) == "2.25";
        check("parse/number_and_value_text", numbers);
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
