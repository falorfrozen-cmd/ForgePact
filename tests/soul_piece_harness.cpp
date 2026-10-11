// Behavioral regression harness for Boss soul pieces (SoulPieceMod.hpp,
// issue #197).
//
// The Python runner splices the REAL ForgePact::SoulPieces header in below.
// Only the game is replaced: each scenario asks the core what the adapter in
// ModuleMain.cpp asks it (the gem roll's drop decision with the room's name
// and act, the dying source's ancestry, the Gem of Incarnation's rate and a
// roll; a right-click's use decision; the stack veto after the game answered
// that two items stack), with the values the runtime hands over. No game
// process is touched.
//
// The baselines pin the mod off as the vanilla game: no drop for any input,
// no summon, no veto. The targets pin the chance (five quarters of the gem's
// own chance on the same roll) and every refusal, each "never" beside an
// input that does give a piece, so a decision that refuses everything fails
// them instead of passing them.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// PRODUCTION_SOULPIECES

namespace SP = ForgePact::SoulPieces;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string W(SP::DropVerdict v) { return SP::Word(v); }
static std::string W(SP::UseVerdict v) { return SP::Word(v); }

// Rooms as the act table and the name rules sort them: ordinary zones, side
// dungeons and rifts drop; towns, `_Boss` rooms, `Uber_` and `Dev_` rooms do not.
struct Room { const char* name; int act; };
static const Room kEligible[] = {
    { "Act_01_01", 1 }, { "Pumpkin_Cellar_01_rm", 1 }, { "Unstable_Rift_01_03", 1 },
    { "Act_02_03", 2 }, { "Frozen_Cellar_01_rm", 2 }, { "Act_07_04_02", 7 },
    { "Distorted_Horizons_02_rm", 7 }, { "Act_09_Wasp_Nest_rm", 9 },
};
static const Room kTowns[] = { { "Town_01_rm", 1 }, { "Town_02_rm", 2 }, { "Town_09_rm", 9 } };
static const Room kNameRule[] = {
    { "Act_01_Boss_Dungeon_01", 1 }, { "Fields_Boss_rm", 1 }, { "Act_02_Boss", 2 },
    { "Act_09_Treasure_Dungeon_Boss_rm", 9 }, { "Uber_Inoya_rm", 1 },
    { "Dev_5_Mika_Test_Abyss_Realm_rm", 9 },
};
static const double kRolls[] = { 0.0, 0.0000001, 0.5, 0.999999 };
static const double kRates[] = { 10000.0, 9000.0, 1.0, 0.0, -5.0 };

// Counts the hits of 1 in `d` over the deterministic grid i / n, i < n.
static long GridHits(double chance, long n) {
    long hits = 0;
    for (long i = 0; i < n; ++i) if (static_cast<double>(i) / static_cast<double>(n) < chance) ++hits;
    return hits;
}

int main() {
    // ---- baseline: the mod off is the vanilla game ----------------------------
    {
        // Every input, eligible or not, forced or not, at any rate and roll: off.
        long cases = 0, bad = 0;
        std::string first;
        auto all = [&](const Room* rooms, size_t n) {
            for (size_t r = 0; r < n; ++r)
                for (int forced = 0; forced < 2; ++forced)
                    for (int monster = 0; monster < 2; ++monster)
                        for (int boss = 0; boss < 2; ++boss)
                            for (double g : kRates)
                                for (double roll : kRolls) {
                                    ++cases;
                                    SP::DropVerdict v = SP::DecideDrop(false, forced != 0, rooms[r].name, rooms[r].act,
                                        monster != 0, boss != 0, g, roll);
                                    if (v != SP::DropVerdict::Off) { if (!bad++) first = std::string(rooms[r].name) + "->" + W(v); }
                                }
        };
        all(kEligible, std::size(kEligible));
        all(kTowns, std::size(kTowns));
        all(kNameRule, std::size(kNameRule));
        const Room noAct[] = { { "Act_01_01", 0 }, { "Satanic_Zone_rm", 0 }, { "Act_01_01", 10 } };
        all(noAct, std::size(noAct));
        check("baseline/off-never-drops", bad == 0 && cases > 0,
            "cases=" + std::to_string(cases) + " notOff=" + std::to_string(bad) + (first.empty() ? "" : " first=" + first));
    }
    {
        // A right-click on a piece of any act, in any room, while off: off.
        long bad = 0;
        std::string first;
        const char* rooms[] = { "Act_01_01", "Act_02_Boss", "Town_01_rm", "Uber_Inoya_rm" };
        for (const char* room : rooms)
            for (int act = 0; act <= SP::kActCount + 1; ++act) {
                SP::UseVerdict v = SP::DecideUse(false, room, act);
                if (v != SP::UseVerdict::Off) { if (!bad++) first = std::string(room) + ":" + std::to_string(act) + "->" + W(v); }
            }
        check("baseline/off-never-summons", bad == 0, "notOff=" + std::to_string(bad) + (first.empty() ? "" : " first=" + first));
    }
    {
        // Two items the game said stack are never split while off, pieces or not.
        long bad = 0;
        const long long seeds[] = { 0, 12345, 777001, 777003, SP::SeedOfAct(1), SP::SeedOfAct(2), SP::SeedOfAct(7), SP::SeedOfAct(9) };
        for (long long a : seeds) for (long long b : seeds) if (SP::VetoStack(false, a, b)) ++bad;
        check("baseline/off-never-vetoes", bad == 0, "vetoed=" + std::to_string(bad));
    }
    {
        // A fresh state is off and unforced, and says so.
        SP::State s;
        SP::StatusInputs in;
        in.room = "Act_01_01"; in.act = 1; in.gemRate = 10000.0;
        const std::string line = SP::StatusLine(s, in);
        check("baseline/default-off", !s.on && !s.forced && line.rfind("soulpiece: off force=off ", 0) == 0, line);
    }

    // ---- targets ------------------------------------------------------------------
    {
        // Five quarters of the gem's chance: D = floor(G x 4 / 5), at least 1.
        struct Case { double g; long long d; };
        const Case cases[] = { { 10000.0, 8000 }, { 9000.0, 7200 }, { 11000.0, 8800 }, { 7.0, 5 }, { 1.0, 1 },
                               { 0.0, 0 }, { -1.0, 0 }, { std::nan(""), 0 } };
        bool ok = true;
        std::string detail;
        for (const Case& c : cases) {
            const long long d = SP::PieceDenominator(c.g);
            if (d != c.d) { ok = false; detail += " D(" + std::to_string(c.g) + ")=" + std::to_string(d); }
        }
        // On one grid of rolls the piece hits exactly 5/4 as often as the gem
        // where G is a multiple of 5.
        const long n = 80000;
        const long pieces = GridHits(1.0 / static_cast<double>(SP::PieceDenominator(10000.0)), n);
        const long gems = GridHits(1.0 / 10000.0, n);
        long decided = 0;
        for (long i = 0; i < n; ++i)
            if (SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 10000.0, static_cast<double>(i) / n) == SP::DropVerdict::Hit)
                ++decided;
        ok = ok && pieces == 10 && gems == 8 && decided == pieces && decided * 4 == gems * 5;
        // The edge: just under 1/D hits, 1/D itself misses.
        const double edge = 1.0 / 8000.0;
        ok = ok && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 10000.0, std::nextafter(edge, 0.0)) == SP::DropVerdict::Hit
                && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 10000.0, edge) == SP::DropVerdict::Miss
                && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 10000.0, 0.5) == SP::DropVerdict::Miss;
        // More gem rolls give proportionally more pieces: 1 in D of each.
        const long more = [] { long h = 0; for (long i = 0; i < 160000; ++i)
            if (SP::DecideDrop(true, false, "Act_02_01", 2, true, false, 10000.0, static_cast<double>(i) / 160000) == SP::DropVerdict::Hit) ++h; return h; }();
        ok = ok && more == 2 * decided;
        // An unknown rate gives nothing.
        ok = ok && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 0.0, 0.0) == SP::DropVerdict::NoRate
                && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, -3.0, 0.0) == SP::DropVerdict::NoRate;
        check("target/chance-five-quarters", ok,
            "pieces=" + std::to_string(pieces) + " gems=" + std::to_string(gems) + " decided=" + std::to_string(decided)
                + " more=" + std::to_string(more) + detail);
    }
    {
        // Forced: every eligible gem roll gives a piece whatever the roll or the
        // rate, and none of the first seven refusals is overridden.
        bool ok = true;
        std::string detail;
        for (const Room& r : kEligible)
            for (double g : kRates)
                for (double roll : kRolls) {
                    SP::DropVerdict v = SP::DecideDrop(true, true, r.name, r.act, true, false, g, roll);
                    if (v != SP::DropVerdict::Hit) { ok = false; detail = std::string(r.name) + "->" + W(v); }
                }
        ok = ok && SP::DecideDrop(true, true, "Town_01_rm", 1, true, false, 10000.0, 0.0) == SP::DropVerdict::Town
                && SP::DecideDrop(true, true, "Act_01_Boss_Dungeon_01", 1, true, false, 10000.0, 0.0) == SP::DropVerdict::BossRoom
                && SP::DecideDrop(true, true, "Act_01_01", 0, true, false, 10000.0, 0.0) == SP::DropVerdict::NoAct
                && SP::DecideDrop(true, true, "Act_01_01", 1, false, false, 10000.0, 0.0) == SP::DropVerdict::NotMonster
                && SP::DecideDrop(true, true, "Act_01_01", 1, true, true, 10000.0, 0.0) == SP::DropVerdict::BossKill
                && SP::DecideDrop(false, true, "Act_01_01", 1, true, false, 10000.0, 0.0) == SP::DropVerdict::Off;
        check("target/forced-ignores-chance-not-eligibility", ok, detail);
    }
    {
        // A boss's own death never drops a piece, at any roll, forced or not;
        // the same kill by an ordinary monster does.
        bool ok = SP::DecideDrop(true, false, "Act_01_01", 1, true, false, 10000.0, 0.0) == SP::DropVerdict::Hit;
        for (double roll : kRolls)
            for (int forced = 0; forced < 2; ++forced)
                ok = ok && SP::DecideDrop(true, forced != 0, "Act_01_01", 1, true, true, 10000.0, roll) == SP::DropVerdict::BossKill;
        check("target/boss-kill-never-drops", ok);
    }
    {
        // A chest, a destructible or a goblin pile reaches the gem roll too: never a piece.
        bool ok = SP::DecideDrop(true, false, "Act_02_01", 2, true, false, 10000.0, 0.0) == SP::DropVerdict::Hit;
        for (double roll : kRolls)
            for (int forced = 0; forced < 2; ++forced)
                for (int boss = 0; boss < 2; ++boss)
                    ok = ok && SP::DecideDrop(true, forced != 0, "Act_02_01", 2, false, boss != 0, 10000.0, roll) == SP::DropVerdict::NotMonster;
        check("target/not-monster-never-drops", ok);
    }
    {
        // Towns are `town`; `_Boss`, `Uber_` and `Dev_` rooms are `boss-room`;
        // a room outside the act table is `no-act`; every eligible room hits at roll 0.
        bool ok = true;
        std::string detail;
        for (const Room& r : kEligible) {
            SP::DropVerdict refusal = SP::DropVerdict::Off;
            const bool eligible = SP::RoomEligible(r.name, r.act, refusal);
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, 10000.0, 0.0);
            if (!eligible || v != SP::DropVerdict::Hit) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        for (const Room& r : kTowns) {
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, 10000.0, 0.0);
            if (v != SP::DropVerdict::Town) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        for (const Room& r : kNameRule) {
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, 10000.0, 0.0);
            if (v != SP::DropVerdict::BossRoom) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        const SP::DropVerdict none = SP::DecideDrop(true, false, "Satanic_Zone_rm", 0, true, false, 10000.0, 0.0);
        const SP::DropVerdict past = SP::DecideDrop(true, false, "Act_01_01", 10, true, false, 10000.0, 0.0);
        ok = ok && none == SP::DropVerdict::NoAct && past == SP::DropVerdict::NoAct;
        check("target/town-and-boss-rooms-refused", ok, detail);
    }
    {
        // Use: refused off and in a town; any other room summons that act's boss.
        bool ok = SP::DecideUse(true, "Town_01_rm", 1) == SP::UseVerdict::Town
               && SP::DecideUse(true, "Town_02_rm", 2) == SP::UseVerdict::Town
               && SP::DecideUse(false, "Act_01_01", 1) == SP::UseVerdict::Off
               && SP::DecideUse(true, "Act_01_01", 0) == SP::UseVerdict::NotPiece
               && SP::DecideUse(true, "Act_01_01", SP::kActCount + 1) == SP::UseVerdict::NotPiece
               && SP::DecideUse(true, "Act_01_01", 1) == SP::UseVerdict::Summon
               && SP::DecideUse(true, "Act_02_Boss", 2) == SP::UseVerdict::Summon
               && SP::DecideUse(true, "Uber_Inoya_rm", 1) == SP::UseVerdict::Summon
               && SP::DecideUse(true, "Act_07_01", 7) == SP::UseVerdict::Summon;
        check("target/use-refused-in-town-and-off", ok);
    }
    {
        // Nine acts, in order, each with a named boss object and a "Piece of" name;
        // Act 7 is Mevius, Act 1 Gurag, Act 2 the Grim Reaper.
        bool ok = std::size(SP::kActs) == 9 && SP::kActCount == 9;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const SP::ActBoss* b = SP::BossOfAct(act);
            if (!b || b->act != act || !b->named || !b->bossObject || !b->pieceName
                || !std::string_view(b->bossObject).ends_with("_obj")
                || !std::string_view(b->pieceName).starts_with("Piece of ")
                || !std::string_view(b->pieceName).ends_with(" Soul")) { ok = false; detail += " act" + std::to_string(act); }
        }
        ok = ok && SP::BossOfAct(0) == nullptr && SP::BossOfAct(10) == nullptr && SP::BossOfAct(-1) == nullptr
                && std::string_view(SP::BossOfAct(1)->bossObject) == "Gurag_obj"
                && std::string_view(SP::BossOfAct(2)->bossObject) == "Reaper_obj"
                && std::string_view(SP::BossOfAct(7)->bossObject) == "Mevius_obj"
                && std::string_view(SP::BossOfAct(7)->pieceName) == "Piece of Mevius' Soul"
                && std::string_view(SP::BossOfAct(2)->pieceName) == "Piece of the Grim Reaper's Soul";
        // Every act drops and summons: no act reaches `unnamed`.
        for (int act = 1; act <= SP::kActCount; ++act)
            ok = ok && SP::DecideUse(true, "Act_01_01", act) == SP::UseVerdict::Summon
                    && SP::DecideDrop(true, true, "Act_01_01", act, true, false, 10000.0, 0.0) == SP::DropVerdict::Hit;
        check("target/every-act-has-a-boss", ok, detail);
    }
    {
        // While on, the game's `true` is split when exactly one item is a piece or
        // two pieces' acts differ; pieces of one act, and two ordinary items, stack.
        const long long p1 = SP::SeedOfAct(1), p1b = SP::SeedOfAct(1), p2 = SP::SeedOfAct(2), plain = 4242;
        bool ok = SP::VetoStack(true, p1, plain) && SP::VetoStack(true, plain, p2)
               && SP::VetoStack(true, p1, p2) && SP::VetoStack(true, p2, p1)
               && !SP::VetoStack(true, p1, p1b) && !SP::VetoStack(true, plain, 777001)
               && !SP::VetoStack(true, plain, plain)
               && !SP::VetoStack(false, p1, p2) && !SP::VetoStack(false, p1, plain);
        check("target/stack-veto-only-while-on", ok);
    }
    {
        // 777101..777109, one per act, and back; the signature items' seeds are not pieces.
        bool ok = SP::kSeedBase == 777100;
        for (int act = 1; act <= SP::kActCount; ++act)
            ok = ok && SP::SeedOfAct(act) == 777100 + act && SP::ActOfSeed(SP::SeedOfAct(act)) == act && SP::IsPiece(SP::SeedOfAct(act));
        const long long notPieces[] = { 0, -777101, 777001, 777002, 777003, 777100, 777110, 777199, 1777101 };
        for (long long seed : notPieces) ok = ok && SP::ActOfSeed(seed) == 0 && !SP::IsPiece(seed);
        ok = ok && SP::SeedOfAct(0) == 0 && SP::SeedOfAct(10) == 0;
        check("target/seed-round-trip", ok);
    }
    {
        // The counters by verdict and act, and the status line built from them.
        SP::State s;
        bool ok = SP::Apply(s, SP::ParseCommand("1")) && s.on && !s.forced;
        SP::RecordDrop(s, SP::DropVerdict::Hit, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 2);
        SP::RecordDrop(s, SP::DropVerdict::Town, 1);
        SP::RecordUse(s, SP::UseVerdict::Summon, 2, true);
        SP::RecordUse(s, SP::UseVerdict::Summon, 1, false);
        SP::RecordUse(s, SP::UseVerdict::Town, 1, false);
        SP::Held held;
        SP::AddHeld(held, SP::SeedOfAct(1), 3);
        SP::AddHeld(held, SP::SeedOfAct(2), 1);
        SP::AddHeld(held, SP::SeedOfAct(2), 2);
        SP::AddHeld(held, 4242, 5);
        SP::StatusInputs in;
        in.room = "Act_01_01"; in.act = 1; in.gemRate = 10000.0;
        in.held = SP::HeldText(held); in.gemRoute = "both"; in.useScript = "PotionUse"; in.useRoute = "both"; in.stackRoute = "both";
        const std::string line = SP::StatusLine(s, in);
        const std::string want = "soulpiece: on force=off room=Act_01_01 act=1 eligible=yes gemrate=10000 chance=1/8000"
            " gemrolls=4 drops=1 misses=2 uses=1 summoned=Reaper_obj refused=town=1,use-town=1,spawn-failed=1"
            " held=1:3/1,2:3/2 hooks=gem:both use:PotionUse:both stack:both";
        ok = ok && line == want && s.counters.dropsByAct[1] == 1 && s.counters.summonsByAct[2] == 1;
        SP::State off;
        SP::StatusInputs town;
        town.room = "Town_01_rm"; town.act = 1; town.gemRate = 0.0; town.gemRateWhy = "unread";
        const std::string offLine = SP::StatusLine(off, town);
        const std::string wantOff = "soulpiece: off force=off room=Town_01_rm act=1 eligible=no(town) gemrate=unavailable(unread)"
            " chance=1/- gemrolls=0 drops=0 misses=0 uses=0 summoned=- refused=- held=- hooks=gem:none use:none:none stack:none";
        ok = ok && offLine == wantOff;
        check("command/status-line", ok, "[" + line + "] [" + offLine + "]");
    }
    {
        // The verb's words: 1|0|on|off|status|force on|force off; anything else refused.
        SP::State s;
        bool ok = SP::ParseCommand("1") == SP::Command::On && SP::ParseCommand("on") == SP::Command::On
               && SP::ParseCommand("0") == SP::Command::Off && SP::ParseCommand("off") == SP::Command::Off
               && SP::ParseCommand("") == SP::Command::Status && SP::ParseCommand("status") == SP::Command::Status
               && SP::ParseCommand("force on") == SP::Command::ForceOn && SP::ParseCommand("  force   off ") == SP::Command::ForceOff
               && SP::ParseCommand("2") == SP::Command::Invalid && SP::ParseCommand("force") == SP::Command::Invalid
               && SP::ParseCommand("forceon") == SP::Command::Invalid;
        ok = ok && SP::Apply(s, SP::Command::ForceOn) && s.forced && !s.on
                && SP::Apply(s, SP::Command::On) && s.on
                && SP::Apply(s, SP::Command::Off) && !s.on && s.forced
                && SP::Apply(s, SP::Command::ForceOff) && !s.forced
                && SP::Apply(s, SP::Command::Status) && !SP::Apply(s, SP::Command::Invalid);
        check("command/words-parsed", ok);
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
