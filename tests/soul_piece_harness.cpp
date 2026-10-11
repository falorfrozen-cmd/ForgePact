// Behavioral regression harness for Boss soul pieces (SoulPieceMod.hpp,
// issue #197).
//
// The Python runner splices the REAL ForgePact::SoulPieces header in below.
// Only the game is replaced: each scenario asks the core what the adapter in
// ModuleMain.cpp asks it (the gem roll's drop decision with the room's name
// and act, the dying source's ancestry, the Gem of Incarnation's rate and a
// roll; the act from `RoomActIndex`'s answer; a right-click's use decision with
// the room's name and act and the piece's act; the portal's target and place),
// with the values the runtime hands over. No game process is touched.
//
// The baselines pin the mod off as the vanilla game: no drop and no portal for
// any input, a fresh state that is off, and the act that `RoomActIndex`
// answered as Live procedure 1 measured it (the answer is the act). The
// targets pin the chance at the measured gem rate (five quarters of the gem's
// own chance on the same roll) and every refusal, each "never" beside an
// input that does give a piece or a portal, so a decision that refuses
// everything fails them instead of passing them.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
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
// Rooms `RoomActIndex` leaves undefined: the act boss rooms, uber realms,
// Chaos Tower and the like (act 0), and an answer past the table.
static const Room kNoAct[] = {
    { "Act_01_Boss", 0 }, { "Act_02_Boss", 0 }, { "Satanic_Zone_rm", 0 }, { "Uber_Inoya_rm", 0 },
    { "Act_01_01", 10 },
};
static const double kRolls[] = { 0.0, 0.0000001, 0.5, 0.999999 };
// The measured rate first (Live procedure 1), then other bases and unknowns.
static const double kRates[] = { 9000.0, 10000.0, 11000.0, 1.0, 0.0, -5.0 };
static const double kMeasuredRate = 9000.0;

// Counts the hits of `chance` over the deterministic grid i / n, i < n.
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
        all(kNoAct, std::size(kNoAct));
        // The control: the same eligible input with the mod on gives a piece.
        const bool control = SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Hit;
        check("baseline/off-never-drops", bad == 0 && cases > 0 && control,
            "cases=" + std::to_string(cases) + " notOff=" + std::to_string(bad) + " control=" + (control ? "hit" : "no")
                + (first.empty() ? "" : " first=" + first));
    }
    {
        // A right-click on any item, a piece of any act or none, in any room, while off: off.
        long cases = 0, bad = 0;
        std::string first;
        auto all = [&](const Room* rooms, size_t n) {
            for (size_t r = 0; r < n; ++r)
                for (int pieceAct = -1; pieceAct <= SP::kActCount + 1; ++pieceAct) {
                    ++cases;
                    SP::UseVerdict v = SP::DecideUse(false, rooms[r].name, rooms[r].act, pieceAct);
                    if (v != SP::UseVerdict::Off) {
                        if (!bad++) first = std::string(rooms[r].name) + ":" + std::to_string(pieceAct) + "->" + W(v);
                    }
                }
        };
        all(kEligible, std::size(kEligible));
        all(kTowns, std::size(kTowns));
        all(kNameRule, std::size(kNameRule));
        all(kNoAct, std::size(kNoAct));
        // The control: the same piece in the same town with the mod on opens a portal.
        const bool control = SP::DecideUse(true, "Town_01_rm", 1, 1) == SP::UseVerdict::Portal;
        check("baseline/off-never-opens-a-portal", bad == 0 && cases > 0 && control,
            "cases=" + std::to_string(cases) + " notOff=" + std::to_string(bad) + " control=" + (control ? "portal" : "no")
                + (first.empty() ? "" : " first=" + first));
    }
    {
        // `RoomActIndex` answers the act itself (Live procedure 1: 1.0 in Town_01_rm
        // and Act_01_01, 2.0 in Act_02_01). Anything that is not a whole number
        // from 1 to 9, and an answer that is not a number at all (undefined
        // outside the switch), is no act.
        struct Case { const char* where; bool isNumber; double answer; int act; };
        const Case cases[] = {
            { "Town_01_rm", true, 1.0, 1 }, { "Act_01_01", true, 1.0, 1 }, { "Act_02_01", true, 2.0, 2 },
            { "act7", true, 7.0, 7 }, { "act9", true, 9.0, 9 },
            { "undefined", false, 1.0, 0 }, { "undefined0", false, 0.0, 0 },
            { "zero", true, 0.0, 0 }, { "ten", true, 10.0, 0 }, { "half", true, 1.5, 0 }, { "negative", true, -1.0, 0 },
            { "nan", true, std::nan(""), 0 }, { "inf", true, std::numeric_limits<double>::infinity(), 0 },
        };
        bool ok = true;
        std::string detail;
        for (const Case& c : cases) {
            const int got = SP::ActOfRoomActIndex(c.isNumber, c.answer);
            if (got != c.act) { ok = false; detail += std::string(" ") + c.where + "=" + std::to_string(got); }
        }
        // Every act from 1 to 9 maps to itself.
        for (int act = 1; act <= SP::kActCount; ++act)
            if (SP::ActOfRoomActIndex(true, static_cast<double>(act)) != act) { ok = false; detail += " act" + std::to_string(act); }
        check("baseline/room-act-index-is-the-act", ok, detail);
    }
    {
        // A fresh state is off and unforced, and says so.
        SP::State s;
        SP::StatusInputs in;
        in.room = "Act_01_01"; in.act = 1; in.gemRate = kMeasuredRate;
        const std::string line = SP::StatusLine(s, in);
        check("baseline/default-off", !s.on && !s.forced && line.rfind("soulpiece: off force=off ", 0) == 0, line);
    }

    // ---- targets ------------------------------------------------------------------
    {
        // Five quarters of the gem's chance: D = floor(G x 4 / 5), at least 1.
        struct Case { double g; long long d; };
        const Case cases[] = { { 9000.0, 7200 }, { 10000.0, 8000 }, { 11000.0, 8800 }, { 7.0, 5 }, { 1.0, 1 },
                               { 0.0, 0 }, { -1.0, 0 }, { std::nan(""), 0 } };
        bool ok = SP::kExpectedGemRate == 9000 && SP::PieceDenominator(static_cast<double>(SP::kExpectedGemRate)) == 7200;
        std::string detail;
        for (const Case& c : cases) {
            const long long d = SP::PieceDenominator(c.g);
            if (d != c.d) { ok = false; detail += " D(" + std::to_string(c.g) + ")=" + std::to_string(d); }
        }
        // On one grid of rolls at the measured rate the piece hits exactly 5/4
        // as often as the gem: 10 against 8 over 72,000 rolls.
        const long n = 72000;
        const long pieces = GridHits(1.0 / static_cast<double>(SP::PieceDenominator(kMeasuredRate)), n);
        const long gems = GridHits(1.0 / kMeasuredRate, n);
        long decided = 0;
        for (long i = 0; i < n; ++i)
            if (SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, static_cast<double>(i) / n) == SP::DropVerdict::Hit)
                ++decided;
        ok = ok && pieces == 10 && gems == 8 && decided == pieces && decided * 4 == gems * 5;
        // The edge: just under 1/D hits, 1/D itself misses.
        const double edge = 1.0 / 7200.0;
        ok = ok && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, std::nextafter(edge, 0.0)) == SP::DropVerdict::Hit
                && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, edge) == SP::DropVerdict::Miss
                && SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, 0.5) == SP::DropVerdict::Miss;
        // More gem rolls give proportionally more pieces: 1 in D of each.
        const long more = [] { long h = 0; for (long i = 0; i < 144000; ++i)
            if (SP::DecideDrop(true, false, "Act_02_01", 2, true, false, kMeasuredRate, static_cast<double>(i) / 144000) == SP::DropVerdict::Hit) ++h; return h; }();
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
        ok = ok && SP::DecideDrop(true, true, "Town_01_rm", 1, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Town
                && SP::DecideDrop(true, true, "Act_01_Boss_Dungeon_01", 1, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::BossRoom
                && SP::DecideDrop(true, true, "Act_01_01", 0, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::NoAct
                && SP::DecideDrop(true, true, "Act_01_01", 1, false, false, kMeasuredRate, 0.0) == SP::DropVerdict::NotMonster
                && SP::DecideDrop(true, true, "Act_01_01", 1, true, true, kMeasuredRate, 0.0) == SP::DropVerdict::BossKill
                && SP::DecideDrop(false, true, "Act_01_01", 1, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Off;
        check("target/forced-ignores-chance-not-eligibility", ok, detail);
    }
    {
        // A boss's own death never drops a piece, at any roll, forced or not;
        // the same kill by an ordinary monster does.
        bool ok = SP::DecideDrop(true, false, "Act_01_01", 1, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Hit;
        for (double roll : kRolls)
            for (int forced = 0; forced < 2; ++forced)
                ok = ok && SP::DecideDrop(true, forced != 0, "Act_01_01", 1, true, true, kMeasuredRate, roll) == SP::DropVerdict::BossKill;
        check("target/boss-kill-never-drops", ok);
    }
    {
        // A chest, a destructible or a goblin pile reaches the gem roll too: never a piece.
        bool ok = SP::DecideDrop(true, false, "Act_02_01", 2, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Hit;
        for (double roll : kRolls)
            for (int forced = 0; forced < 2; ++forced)
                for (int boss = 0; boss < 2; ++boss)
                    ok = ok && SP::DecideDrop(true, forced != 0, "Act_02_01", 2, false, boss != 0, kMeasuredRate, roll) == SP::DropVerdict::NotMonster;
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
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, kMeasuredRate, 0.0);
            if (!eligible || v != SP::DropVerdict::Hit) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        for (const Room& r : kTowns) {
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, kMeasuredRate, 0.0);
            if (v != SP::DropVerdict::Town) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        for (const Room& r : kNameRule) {
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, kMeasuredRate, 0.0);
            if (v != SP::DropVerdict::BossRoom) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        for (const Room& r : kNoAct) {
            SP::DropVerdict v = SP::DecideDrop(true, false, r.name, r.act, true, false, kMeasuredRate, 0.0);
            if (v != SP::DropVerdict::NoAct) { ok = false; detail += std::string(" ") + r.name + "->" + W(v); }
        }
        check("target/town-and-boss-rooms-refused", ok, detail);
    }
    {
        // Use, the mod on: a piece of any act opens a portal from a town and from
        // any zone of an act, whichever act the room and the piece belong to.
        bool ok = true;
        std::string detail;
        long portals = 0;
        for (int pieceAct = 1; pieceAct <= SP::kActCount; ++pieceAct) {
            for (const Room& r : kTowns) {
                SP::UseVerdict v = SP::DecideUse(true, r.name, r.act, pieceAct);
                if (v == SP::UseVerdict::Portal) ++portals;
                else { ok = false; detail += std::string(" ") + r.name + ":" + std::to_string(pieceAct) + "->" + W(v); }
            }
            for (const Room& r : kEligible) {
                SP::UseVerdict v = SP::DecideUse(true, r.name, r.act, pieceAct);
                if (v == SP::UseVerdict::Portal) ++portals;
                else { ok = false; detail += std::string(" ") + r.name + ":" + std::to_string(pieceAct) + "->" + W(v); }
            }
        }
        check("target/use-opens-a-portal-in-towns-and-zones", ok && portals > 0, "portals=" + std::to_string(portals) + detail);
    }
    {
        // Use refusals, in DecideUse's order, each beside an input that opens a portal.
        struct Case { bool on; const char* room; int roomAct; int pieceAct; SP::UseVerdict want; };
        const Case cases[] = {
            // off, beside the same use on
            { false, "Town_01_rm", 1, 1, SP::UseVerdict::Off }, { true, "Town_01_rm", 1, 1, SP::UseVerdict::Portal },
            { false, "Act_01_01", 1, 1, SP::UseVerdict::Off }, { true, "Act_01_01", 1, 1, SP::UseVerdict::Portal },
            // not a piece (and off before not-piece)
            { true, "Act_01_01", 1, 0, SP::UseVerdict::NotPiece }, { true, "Act_01_01", 1, 10, SP::UseVerdict::NotPiece },
            { true, "Act_01_01", 1, -1, SP::UseVerdict::NotPiece }, { false, "Act_01_01", 1, 0, SP::UseVerdict::Off },
            // a room with no act (not-piece before no-act)
            { true, "Satanic_Zone_rm", 0, 1, SP::UseVerdict::NoAct }, { true, "Act_01_Boss", 0, 1, SP::UseVerdict::NoAct },
            { true, "Uber_Inoya_rm", 0, 2, SP::UseVerdict::NoAct }, { true, "Act_01_01", 10, 1, SP::UseVerdict::NoAct },
            { true, "Satanic_Zone_rm", 0, 0, SP::UseVerdict::NotPiece },
            // the drop's name rule (no-act before boss-room)
            { true, "Act_01_Boss_Dungeon_01", 1, 1, SP::UseVerdict::BossRoom }, { true, "Fields_Boss_rm", 1, 1, SP::UseVerdict::BossRoom },
            { true, "Act_02_Boss", 2, 2, SP::UseVerdict::BossRoom }, { true, "Uber_Inoya_rm", 1, 1, SP::UseVerdict::BossRoom },
            { true, "Dev_5_Mika_Test_Abyss_Realm_rm", 9, 9, SP::UseVerdict::BossRoom },
            { true, "Act_09_Treasure_Dungeon_Boss_rm", 9, 9, SP::UseVerdict::BossRoom },
            { true, "Act_02_Boss", 0, 2, SP::UseVerdict::NoAct },
            // beside them: an ordinary zone of the same act opens one
            { true, "Act_02_03", 2, 2, SP::UseVerdict::Portal }, { true, "Act_09_Wasp_Nest_rm", 9, 9, SP::UseVerdict::Portal },
        };
        bool ok = true;
        std::string detail;
        for (const Case& c : cases) {
            const SP::UseVerdict v = SP::DecideUse(c.on, c.room, c.roomAct, c.pieceAct);
            if (v != c.want) {
                ok = false;
                detail += std::string(" ") + c.room + ":" + std::to_string(c.roomAct) + ":" + std::to_string(c.pieceAct) + "->" + W(v);
            }
        }
        check("target/use-refused-when-off-outside-acts-and-in-boss-rooms", ok, detail);
    }
    {
        // The portal leads to the piece's act boss room (`Act_0N_Boss`), and stands
        // kPortalOffsetPx to the player's right, mirrored to the left past the
        // room's width; an unknown width (0) keeps it to the right.
        bool ok = SP::kPortalOffsetPx == 96.0;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const std::string want = std::string("Act_0") + std::to_string(act) + "_Boss";
            const char* target = SP::PortalTarget(act);
            if (!target || want != target || std::string_view(SP::BossOfAct(act)->bossRoom) != want) {
                ok = false; detail += " act" + std::to_string(act) + "=" + (target ? target : "null");
            }
        }
        ok = ok && SP::PortalTarget(0) == nullptr && SP::PortalTarget(10) == nullptr && SP::PortalTarget(-1) == nullptr;
        ok = ok && SP::PortalX(1000.0, 4000.0) == 1096.0
                && SP::PortalX(3904.0, 4000.0) == 4000.0
                && SP::PortalX(3950.0, 4000.0) == 3854.0
                && SP::PortalX(1000.0, 0.0) == 1096.0;
        check("target/portal-target-is-the-act-boss-room", ok, detail);
    }
    {
        // Nine acts, in order, each with a named boss object, a "Piece of" name and
        // its boss room; Act 7 is Mevius, Act 1 Gurag, Act 2 the Grim Reaper.
        bool ok = std::size(SP::kActs) == 9 && SP::kActCount == 9;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const SP::ActBoss* b = SP::BossOfAct(act);
            if (!b || b->act != act || !b->named || !b->bossObject || !b->pieceName || !b->bossRoom
                || !std::string_view(b->bossObject).ends_with("_obj")
                || !std::string_view(b->pieceName).starts_with("Piece of ")
                || !std::string_view(b->pieceName).ends_with(" Soul")
                || !std::string_view(b->bossRoom).ends_with("_Boss")) { ok = false; detail += " act" + std::to_string(act); }
        }
        ok = ok && SP::BossOfAct(0) == nullptr && SP::BossOfAct(10) == nullptr && SP::BossOfAct(-1) == nullptr
                && std::string_view(SP::BossOfAct(1)->bossObject) == "Gurag_obj"
                && std::string_view(SP::BossOfAct(2)->bossObject) == "Reaper_obj"
                && std::string_view(SP::BossOfAct(7)->bossObject) == "Mevius_obj"
                && std::string_view(SP::BossOfAct(7)->pieceName) == "Piece of Mevius' Soul"
                && std::string_view(SP::BossOfAct(2)->pieceName) == "Piece of the Grim Reaper's Soul";
        // Every act drops and opens a portal: no act reaches `unnamed`.
        for (int act = 1; act <= SP::kActCount; ++act)
            ok = ok && SP::DecideUse(true, "Act_01_01", 1, act) == SP::UseVerdict::Portal
                    && SP::DecideDrop(true, true, "Act_01_01", act, true, false, kMeasuredRate, 0.0) == SP::DropVerdict::Hit;
        check("target/every-act-has-a-boss", ok, detail);
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
        // A piece is the Edible PAS Logo (category and class 11, the entry found by
        // name) carrying a piece's seed with c 0. The logo itself, a key carrying
        // a piece's seed, a unique entry and an unresolved base are not pieces.
        const long long base = 12;  // Live procedure 1 measured b 12 for the logo
        bool ok = SP::kBaseCategory == 11 && SP::kBaseItemType == 11
               && std::string_view(SP::kBaseRepoName) == "consumable_edible_pas_logo";
        for (int act = 1; act <= SP::kActCount; ++act)
            ok = ok && SP::PieceActOfItem(11, base, 0, SP::SeedOfAct(act), base) == act;
        ok = ok && SP::PieceActOfItem(11, base, 0, 4242, base) == 0          // the plain logo
                && SP::PieceActOfItem(12, base, 0, SP::SeedOfAct(1), base) == 0  // a key with a piece's seed
                && SP::PieceActOfItem(11, 13, 0, SP::SeedOfAct(1), base) == 0    // another consumable
                && SP::PieceActOfItem(11, base, 1, SP::SeedOfAct(1), base) == 0  // the unique repository
                && SP::PieceActOfItem(11, base, 0, SP::SeedOfAct(1), -1) == 0;   // base not resolved yet
        check("target/base-is-the-edible-pas-logo", ok);
    }
    {
        // The counters by verdict and act, and the status line built from them.
        SP::State s;
        bool ok = SP::Apply(s, SP::ParseCommand("1")) && s.on && !s.forced;
        SP::RecordDrop(s, SP::DropVerdict::Hit, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 2);
        SP::RecordDrop(s, SP::DropVerdict::Town, 1);
        SP::RecordUse(s, SP::UseVerdict::Portal, 2, true);
        SP::RecordUse(s, SP::UseVerdict::Portal, 1, false);
        SP::RecordUse(s, SP::UseVerdict::BossRoom, 1, false);
        SP::StatusInputs in;
        in.room = "Act_01_01"; in.act = 1; in.gemRate = kMeasuredRate;
        in.gemRoute = "both"; in.useRoute = "both"; in.portalRoute = "portal-obj";
        const std::string line = SP::StatusLine(s, in);
        const std::string want = "soulpiece: on force=off room=Act_01_01 act=1 eligible=yes gemrate=9000 chance=1/7200"
            " gemrolls=4 drops=1 misses=2 uses=3 portals=1 last=Act_02_Boss refused=town=1,use-boss-room=1,portal-failed=1"
            " hooks=gem:both use:PotionUse:both portal:portal-obj";
        ok = ok && line == want && s.counters.dropsByAct[1] == 1 && s.counters.portalsByAct[2] == 1
                && s.counters.portalsByAct[1] == 0 && s.counters.portalFailed == 1;
        SP::State off;
        SP::StatusInputs town;
        town.room = "Town_01_rm"; town.act = 1; town.gemRate = 0.0; town.gemRateWhy = "unread";
        const std::string offLine = SP::StatusLine(off, town);
        const std::string wantOff = "soulpiece: off force=off room=Town_01_rm act=1 eligible=no(town) gemrate=unavailable(unread)"
            " chance=1/- gemrolls=0 drops=0 misses=0 uses=0 portals=0 last=- refused=- hooks=gem:none use:PotionUse:none portal:portal-obj";
        ok = ok && offLine == wantOff;
        SP::StatusInputs gotoRoute;
        gotoRoute.room = "Act_02_01"; gotoRoute.act = 2; gotoRoute.gemRate = kMeasuredRate; gotoRoute.portalRoute = "goto";
        ok = ok && SP::StatusLine(off, gotoRoute).ends_with(" portal:goto");
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
