// Behavioral regression harness for Boss soul pieces (SoulPieceMod.hpp,
// issue #197).
//
// The Python runner splices the REAL ForgePact::SoulPieces header in below.
// Only the game is replaced: each scenario asks the core what the adapter in
// ModuleMain.cpp asks it (the gem roll's drop decision with the room's name
// and act, the dying source's ancestry, the Gem of Incarnation's rate and a
// roll; the act from `RoomActIndex`'s answer; a right-click's use decision with
// the room's name and act and the piece's act; the transfer's target and what
// the frame tick reports back), with the values the runtime hands over. No
// game process is touched.
//
// The baselines pin the mod off as the vanilla game: no drop and no transfer
// for any input, a fresh state that is off, and the act that `RoomActIndex`
// answered as Live procedure 1 measured it (the answer is the act). The
// targets pin the chance at the measured gem rate (five quarters of the gem's
// own chance on the same roll) and every refusal, each "never" beside an
// input that does give a piece or a transfer, so a decision that refuses
// everything fails them instead of passing them.
//
// The key piece (the follow-up, 2026-10-12) is pinned the same way: its `b`
// for each act, its identity with every signal beside an item lacking each
// one, a save's two-decimal round trip as the game writes reals (each "stays
// apart" beside a value that does stack), the legacy bag piece beside it, the
// take-one by stack count and the display sprite's name. The game's stack
// compare is modelled here as GameMaker's default epsilon (0.00001; whether
// the game changes it is not established).
//
// The bag merge (the owner, after Live procedure 1: pieces of one act stack in
// the bag) is pinned as the `InventoryGridAddToStack` detour asks it: the
// incoming item and the bag's items in walk order. Off it never merges; on, a
// piece joins only the first piece of its own act with room under the game's
// cap of 999, never a real item, and each refusal sits beside an input that
// does merge.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

// The runtime's real compare: equal within the math epsilon (GameMaker's default).
static const double kGameEpsilon = 0.00001;
static bool GameEqual(double x, double y) { return std::fabs(x - y) <= kGameEpsilon; }

// What a save does to a real: `string()` writes a whole number whole and any
// other with two decimals (documented runtime behaviour; Live procedure 1
// measures it), and the load reads that text back.
static double SaveRoundTrip(double v) {
    char text[64];
    if (v == std::floor(v)) std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(v));
    else std::snprintf(text, sizeof(text), "%.2f", v);
    return std::strtod(text, nullptr);
}

// The key base's index as #197 Live 3 measured `keys_bifrost_key`; the core
// takes whatever the adapter resolved by name.
static const long long kKeyBase = 2;
// The legacy base's index as Live procedure 1 measured the logo.
static const long long kLegacyBase = 12;

// Items as the merge detour reads them from the bag: a legacy bag piece of an
// act with a count (`o`; 0 when the item carries none), a key-shape piece of an
// act (class 12, `b` 2.1 .. 2.9), and any other item.
static SP::BagItem LegacyPiece(int act, long long count) {
    return { 11, static_cast<double>(kLegacyBase), 0.0, SP::SeedOfAct(act), count };
}
static SP::BagItem KeyShapePiece(int act) {
    return { 12, static_cast<double>(kKeyBase) + act / 10.0, 0.0, SP::SeedOfAct(act), 1 };
}
static SP::BagItem RealItem(int itemType, double b, long long seed, long long count) {
    return { itemType, b, 0.0, seed, count };
}
static std::string W(SP::MergeVerdict v) { return SP::Word(v); }

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
        // The control: the same piece in the same town with the mod on transfers.
        const bool control = SP::DecideUse(true, "Town_01_rm", 1, 1) == SP::UseVerdict::Transfer;
        check("baseline/off-never-transfers", bad == 0 && cases > 0 && control,
            "cases=" + std::to_string(cases) + " notOff=" + std::to_string(bad) + " control=" + (control ? "transfer" : "no")
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
        // Use, the mod on: a piece of any act transfers from a town and from any
        // zone of an act, whichever act the room and the piece belong to.
        bool ok = true;
        std::string detail;
        long transfers = 0;
        for (int pieceAct = 1; pieceAct <= SP::kActCount; ++pieceAct) {
            for (const Room& r : kTowns) {
                SP::UseVerdict v = SP::DecideUse(true, r.name, r.act, pieceAct);
                if (v == SP::UseVerdict::Transfer) ++transfers;
                else { ok = false; detail += std::string(" ") + r.name + ":" + std::to_string(pieceAct) + "->" + W(v); }
            }
            for (const Room& r : kEligible) {
                SP::UseVerdict v = SP::DecideUse(true, r.name, r.act, pieceAct);
                if (v == SP::UseVerdict::Transfer) ++transfers;
                else { ok = false; detail += std::string(" ") + r.name + ":" + std::to_string(pieceAct) + "->" + W(v); }
            }
        }
        check("target/use-transfers-from-towns-and-zones", ok && transfers > 0, "transfers=" + std::to_string(transfers) + detail);
    }
    {
        // Use refusals, in DecideUse's order, each beside an input that transfers.
        struct Case { bool on; const char* room; int roomAct; int pieceAct; SP::UseVerdict want; };
        const Case cases[] = {
            // off, beside the same use on
            { false, "Town_01_rm", 1, 1, SP::UseVerdict::Off }, { true, "Town_01_rm", 1, 1, SP::UseVerdict::Transfer },
            { false, "Act_01_01", 1, 1, SP::UseVerdict::Off }, { true, "Act_01_01", 1, 1, SP::UseVerdict::Transfer },
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
            // beside them: an ordinary zone of the same act transfers
            { true, "Act_02_03", 2, 2, SP::UseVerdict::Transfer }, { true, "Act_09_Wasp_Nest_rm", 9, 9, SP::UseVerdict::Transfer },
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
        // A piece takes the player to its act's boss room (`Act_0N_Boss`); no act,
        // no target. The tick's report: a transfer carried out counts by act,
        // as `fresh` when the boss room answered a saved state before the reset
        // and as `cleared` too when it was gone after it; one that was not
        // carried out counts as failed and moves `last` nowhere, whatever the
        // zone-state answers were.
        bool ok = true;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const std::string want = std::string("Act_0") + std::to_string(act) + "_Boss";
            const char* target = SP::TransferTarget(act);
            if (!target || want != target || std::string_view(SP::BossOfAct(act)->bossRoom) != want) {
                ok = false; detail += " act" + std::to_string(act) + "=" + (target ? target : "null");
            }
        }
        ok = ok && SP::TransferTarget(0) == nullptr && SP::TransferTarget(10) == nullptr && SP::TransferTarget(-1) == nullptr;
        using ZA = SP::ZoneAnswer;
        SP::State s;
        SP::RecordTransfer(s, 1, true, ZA::No, ZA::No);          // first visit: nothing saved
        SP::RecordTransfer(s, 1, true, ZA::Yes, ZA::No);         // saved, and gone after the reset
        SP::RecordTransfer(s, 1, true, ZA::Yes, ZA::Yes);        // saved, and still there after it
        SP::RecordTransfer(s, 1, true, ZA::Yes, ZA::Unread);     // saved, the second read failed
        SP::RecordTransfer(s, 1, true, ZA::Unread, ZA::No);      // the first read failed
        SP::RecordTransfer(s, 2, false, ZA::Yes, ZA::No);        // not carried out
        SP::RecordTransfer(s, 0, true, ZA::Yes, ZA::No);         // no act
        SP::RecordZoneCallFailed(s);
        ok = ok && s.counters.transfersByAct[1] == 5 && s.counters.transfersByAct[2] == 0 && s.counters.fresh == 3
                && s.counters.cleared == 1 && s.counters.transferFailed == 2 && s.counters.zoneCallFailed == 1
                && s.lastTransferAct == 1;
        detail += " transfers1=" + std::to_string(s.counters.transfersByAct[1]) + " fresh=" + std::to_string(s.counters.fresh)
                + " cleared=" + std::to_string(s.counters.cleared) + " failed=" + std::to_string(s.counters.transferFailed);
        // The words the status line shows for each answer.
        ok = ok && SP::ZoneAnswerText(ZA::Yes, "x") == "yes" && SP::ZoneAnswerText(ZA::No, "x") == "no"
                && SP::ZoneAnswerText(ZA::Unread, "call failed") == "unread(call failed)"
                && SP::ZoneAnswerText(ZA::Unread, "") == "unread(?)";
        check("target/transfer-target-is-the-act-boss-room", ok, detail);
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
        // Every act drops and transfers: no act reaches `unnamed`.
        for (int act = 1; act <= SP::kActCount; ++act)
            ok = ok && SP::DecideUse(true, "Act_01_01", 1, act) == SP::UseVerdict::Transfer
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
        // A legacy bag piece (the first builds' drops) is the Edible PAS Logo
        // (category and class 11, the entry found by name) carrying a piece's seed
        // with c 0, and is still recognised beside the key piece. The logo itself,
        // a key carrying a piece's seed, a unique entry and an unresolved base are
        // not pieces; a fractional `b` on the logo is not a legacy piece either.
        const long long base = kLegacyBase;
        bool ok = SP::kBaseCategory == 11 && SP::kBaseItemType == 11
               && std::string_view(SP::kBaseRepoName) == "consumable_edible_pas_logo";
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const long long seed = SP::SeedOfAct(act);
            if (SP::PieceActOfItem(11, base, 0, seed, base) != act
                || SP::AnyPieceActOfItem(11, static_cast<double>(base), 0.0, seed, kKeyBase, base) != act
                || SP::KeyPieceActOfItem(11, static_cast<double>(base), 0.0, seed, base) != 0) {
                ok = false; detail += " act" + std::to_string(act);
            }
        }
        ok = ok && SP::PieceActOfItem(11, base, 0, 4242, base) == 0          // the plain logo
                && SP::PieceActOfItem(12, base, 0, SP::SeedOfAct(1), base) == 0  // a key with a piece's seed
                && SP::PieceActOfItem(11, 13, 0, SP::SeedOfAct(1), base) == 0    // another consumable
                && SP::PieceActOfItem(11, base, 1, SP::SeedOfAct(1), base) == 0  // the unique repository
                && SP::PieceActOfItem(11, base, 0, SP::SeedOfAct(1), -1) == 0;   // base not resolved yet
        // Asked through the question that knows both shapes (the key piece's side
        // is the identity scenario's): the legacy rule's near misses stay refused.
        ok = ok && SP::AnyPieceActOfItem(11, 12.0, 0.0, 4242, kKeyBase, base) == 0                    // the plain logo
                && SP::AnyPieceActOfItem(11, 12.5, 0.0, SP::SeedOfAct(1), kKeyBase, base) == 0        // a fractional logo
                && SP::AnyPieceActOfItem(11, 12.0, 0.0, SP::SeedOfAct(1), kKeyBase, -1) == 0;         // legacy base unresolved
        check("target/legacy-bag-piece-still-recognised", ok, detail);
    }
    {
        // A key piece's `b` is the key base's index + act/10: 2.1 for Act 1 ..
        // 2.9 for Act 9 on Bifrost (index 2). It is never whole, it truncates to
        // the base (so the repository lookup loads the key), pieces of one act
        // compare equal (stack), and every other act and the real key differ by
        // more than the game's epsilon (stay apart). No act or no base: -1.
        bool ok = SP::kKeyBaseCategory == 12 && SP::kKeyBaseItemType == 12
               && std::string_view(SP::kKeyBaseRepoName) == "keys_bifrost_key"
               && SP::kActFractionDivisor == 10.0;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const double b = SP::KeyPieceB(kKeyBase, act);
            const double want = 2.0 + act / 10.0;
            const bool good = std::fabs(b - want) < 1e-9 && b != std::floor(b) && std::trunc(b) == 2.0
                && GameEqual(b, SP::KeyPieceB(kKeyBase, act)) && !GameEqual(b, static_cast<double>(kKeyBase));
            if (!good) { ok = false; detail += " act" + std::to_string(act) + "=" + std::to_string(b); }
            for (int other = 1; other <= SP::kActCount; ++other)
                if (other != act && GameEqual(b, SP::KeyPieceB(kKeyBase, other))) {
                    ok = false; detail += " act" + std::to_string(act) + "~act" + std::to_string(other);
                }
        }
        // Another base moves with its index (resolved by name, never pinned).
        ok = ok && std::fabs(SP::KeyPieceB(7, 3) - 7.3) < 1e-9 && std::trunc(SP::KeyPieceB(7, 9)) == 7.0;
        ok = ok && SP::KeyPieceB(kKeyBase, 0) == -1.0 && SP::KeyPieceB(kKeyBase, 10) == -1.0
                && SP::KeyPieceB(-1, 1) == -1.0;
        check("target/key-piece-b-is-the-base-plus-tenths", ok, detail);
    }
    {
        // A key piece needs every signal: class 12, c 0, `b` truncating to the
        // base, `b` not whole, and tenths that match the act its seed marks.
        // Beside each piece, an item lacking exactly one of them.
        bool ok = true;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const long long seed = SP::SeedOfAct(act);
            const double b = SP::KeyPieceB(kKeyBase, act);
            if (SP::KeyPieceActOfItem(12, b, 0.0, seed, kKeyBase) != act) { ok = false; detail += " built" + std::to_string(act); }
            if (SP::KeyPieceActOfItem(12, 2.0 + act / 10.0, 0.0, seed, kKeyBase) != act) { ok = false; detail += " literal" + std::to_string(act); }
        }
        struct Case { const char* what; int t; double b; double c; long long seed; long long base; };
        const Case refused[] = {
            { "consumable-class", 11, 2.1, 0.0, SP::SeedOfAct(1), kKeyBase },
            { "whole-b-plain-control", 12, 2.0, 0.0, SP::SeedOfAct(1), kKeyBase },
            { "other-key", 12, 3.1, 0.0, SP::SeedOfAct(1), kKeyBase },
            { "below-the-base", 12, 1.9, 0.0, SP::SeedOfAct(9), kKeyBase },
            { "ordinary-seed", 12, 2.1, 0.0, 4242, kKeyBase },
            { "signature-seed", 12, 2.1, 0.0, 777001, kKeyBase },
            { "act-mismatch", 12, 2.1, 0.0, SP::SeedOfAct(2), kKeyBase },
            { "c-fraction", 12, 2.1, 0.05, SP::SeedOfAct(1), kKeyBase },
            { "c-unique", 12, 2.1, 1.0, SP::SeedOfAct(1), kKeyBase },
            { "base-unresolved", 12, 2.1, 0.0, SP::SeedOfAct(1), -1 },
            { "nan-b", 12, std::nan(""), 0.0, SP::SeedOfAct(1), kKeyBase },
            { "inf-b", 12, std::numeric_limits<double>::infinity(), 0.0, SP::SeedOfAct(1), kKeyBase },
            { "nan-c", 12, 2.1, std::nan(""), SP::SeedOfAct(1), kKeyBase },
        };
        for (const Case& c : refused)
            if (SP::KeyPieceActOfItem(c.t, c.b, c.c, c.seed, c.base) != 0) { ok = false; detail += std::string(" ") + c.what; }
        // Through the question that knows both shapes: a key piece is found, and
        // the legacy rule is not widened into the key (a whole-b class-12 item
        // with a piece's seed is neither shape).
        ok = ok && SP::AnyPieceActOfItem(12, 2.1, 0.0, SP::SeedOfAct(1), kKeyBase, kLegacyBase) == 1
                && SP::AnyPieceActOfItem(12, 2.0, 0.0, SP::SeedOfAct(1), kKeyBase, kLegacyBase) == 0
                && SP::AnyPieceActOfItem(12, 2.1, 0.0, SP::SeedOfAct(1), -1, kLegacyBase) == 0;
        check("target/key-piece-identity-needs-every-signal", ok, detail);
    }
    {
        // Through a save: `string()` keeps two decimals, so every act's `b` comes
        // back as the same tenths. A reloaded piece is still the piece of its act,
        // still stacks with a fresh one, and still stays apart from the real key
        // (a whole number, written whole) and from every other act. The control:
        // the round trip really rounds, since a finer fraction would not survive.
        bool ok = true;
        std::string detail;
        for (int act = 1; act <= SP::kActCount; ++act) {
            const double fresh = SP::KeyPieceB(kKeyBase, act);
            const double saved = SaveRoundTrip(fresh);
            const bool good = SP::KeyPieceActOfItem(12, saved, SaveRoundTrip(0.0), SP::SeedOfAct(act), kKeyBase) == act
                && GameEqual(saved, fresh) && std::trunc(saved) == 2.0
                && !GameEqual(saved, SaveRoundTrip(static_cast<double>(kKeyBase)));
            if (!good) { ok = false; detail += " act" + std::to_string(act) + "=" + std::to_string(saved); }
            for (int other = 1; other <= SP::kActCount; ++other)
                if (other != act && GameEqual(saved, SaveRoundTrip(SP::KeyPieceB(kKeyBase, other)))) {
                    ok = false; detail += " act" + std::to_string(act) + "~act" + std::to_string(other);
                }
        }
        // Two-decimal values the tolerance must accept, whichever way they round.
        ok = ok && SP::KeyPieceActOfItem(12, 2.104, 0.0, SP::SeedOfAct(1), kKeyBase) == 1
                && SP::KeyPieceActOfItem(12, 2.896, 0.0, SP::SeedOfAct(9), kKeyBase) == 9;
        // The real key round-trips whole and stays the real key.
        ok = ok && SaveRoundTrip(2.0) == 2.0;
        const double finer = 2.123;
        const bool rounds = SaveRoundTrip(finer) == 2.12 && !GameEqual(SaveRoundTrip(finer), finer);
        check("target/key-piece-survives-a-two-decimal-round-trip", ok && rounds,
            detail + " control=" + (rounds ? "rounds" : "no"));
    }
    {
        // The take-one, by stack count: a refusal keeps every piece and answers
        // false; a transfer from a stack of N > 1 answers false (the game keeps
        // the cells) and leaves N - 1; the last piece answers true (the game
        // clears the cell).
        bool ok = true;
        std::string detail;
        for (long long n : { 2LL, 3LL, 10LL, 99LL }) {
            const SP::StackTake t = SP::TakeOne(SP::UseVerdict::Transfer, n);
            if (t.answer || t.count != n - 1) { ok = false; detail += " n" + std::to_string(n); }
        }
        for (long long n : { 1LL, 0LL }) {
            const SP::StackTake t = SP::TakeOne(SP::UseVerdict::Transfer, n);
            if (!t.answer || t.count != n) { ok = false; detail += " last" + std::to_string(n); }
        }
        for (int v = 0; v < static_cast<int>(SP::UseVerdict::Transfer); ++v)
            for (long long n : { 0LL, 1LL, 2LL, 5LL }) {
                const SP::StackTake t = SP::TakeOne(static_cast<SP::UseVerdict>(v), n);
                if (t.answer || t.count != n) { ok = false; detail += " " + W(static_cast<SP::UseVerdict>(v)) + std::to_string(n); }
            }
        // A stack of three used three times: false, false, then true on the last.
        long long count = 3;
        std::string answers;
        for (int i = 0; i < 3; ++i) {
            const SP::StackTake t = SP::TakeOne(SP::UseVerdict::Transfer, count);
            answers += t.answer ? "T" : "F";
            count = t.count;
        }
        ok = ok && answers == "FFT" && count == 1;
        check("target/stack-use-takes-one-and-answers-by-count", ok, "answers=" + answers + detail);
    }
    {
        // The display sprite is the Gem of Incarnation's, named (never an index),
        // written under item info key "11"; its ground companion is `Pickup_` + it.
        const bool ok = std::string_view(SP::kPieceSpriteName) == "Gem_Incarnation_spr"
            && std::string_view(SP::kIconInfoKey) == "11"
            && SP::GroundSpriteName() == "Pickup_Gem_Incarnation_spr";
        check("target/piece-sprite-is-the-gem-sprite-name", ok, SP::GroundSpriteName());
    }
    {
        // The bag merge with the mod off: whatever comes in and whatever the bag
        // holds, `off` and no target. The control: the same piece into the same
        // bag with the mod on merges.
        const std::vector<std::vector<SP::BagItem>> bags = {
            {}, { LegacyPiece(1, 1) }, { RealItem(11, 12.0, 4242, 1), LegacyPiece(1, 3) },
            { LegacyPiece(9, 998) }, { LegacyPiece(1, 999) },
        };
        long cases = 0, bad = 0;
        std::string first;
        for (const auto& bag : bags) {
            for (int act = 1; act <= SP::kActCount; ++act)
                for (const SP::BagItem& in : { LegacyPiece(act, 1), LegacyPiece(act, 0), RealItem(11, 12.0, 4242, 1), KeyShapePiece(act) }) {
                    ++cases;
                    const SP::MergeDecision d = SP::DecideMerge(false, in, kLegacyBase, bag);
                    if (d.verdict != SP::MergeVerdict::Off || d.target != -1) {
                        if (!bad++) first = "act" + std::to_string(act) + "->" + W(d.verdict);
                    }
                }
        }
        const SP::MergeDecision control = SP::DecideMerge(true, LegacyPiece(1, 1), kLegacyBase, { LegacyPiece(1, 1) });
        const bool controlOk = control.verdict == SP::MergeVerdict::Merge && control.target == 0;
        check("baseline/off-never-merges", bad == 0 && cases > 0 && controlOk,
            "cases=" + std::to_string(cases) + " notOff=" + std::to_string(bad) + " control=" + W(control.verdict)
                + (first.empty() ? "" : " first=" + first));
    }
    {
        // A piece joins the first piece of its own act in walk order, never a
        // piece of another act and never a key-shape piece of the same act; with
        // no piece of its act in the bag it answers `no-target`. A count left
        // absent (0) counts as one piece, on either side.
        bool ok = true;
        std::string detail;
        auto expect = [&](const char* what, const SP::MergeDecision& d, SP::MergeVerdict v, int act, long long target, long long count) {
            if (d.verdict != v || d.act != act || d.target != target || d.count != count) {
                ok = false;
                detail += std::string(" ") + what + "=" + W(d.verdict) + ":" + std::to_string(d.act) + ":"
                    + std::to_string(d.target) + ":" + std::to_string(d.count);
            }
        };
        // Another act's piece and a same-act key-shape piece come first: both are passed over.
        const std::vector<SP::BagItem> bag = {
            RealItem(11, 12.0, 4242, 1), LegacyPiece(2, 5), KeyShapePiece(1), LegacyPiece(1, 3), LegacyPiece(1, 1),
        };
        expect("act1", SP::DecideMerge(true, LegacyPiece(1, 1), kLegacyBase, bag), SP::MergeVerdict::Merge, 1, 3, 4);
        expect("act2", SP::DecideMerge(true, LegacyPiece(2, 2), kLegacyBase, bag), SP::MergeVerdict::Merge, 2, 1, 7);
        expect("act3", SP::DecideMerge(true, LegacyPiece(3, 1), kLegacyBase, bag), SP::MergeVerdict::NoTarget, 3, -1, 0);
        // Every act: a bag of every other act's pieces is no target; its own piece appended is the target.
        for (int act = 1; act <= SP::kActCount; ++act) {
            std::vector<SP::BagItem> others;
            for (int other = 1; other <= SP::kActCount; ++other)
                if (other != act) others.push_back(LegacyPiece(other, 1));
            const SP::MergeDecision none = SP::DecideMerge(true, LegacyPiece(act, 1), kLegacyBase, others);
            others.push_back(LegacyPiece(act, 2));
            const SP::MergeDecision own = SP::DecideMerge(true, LegacyPiece(act, 1), kLegacyBase, others);
            if (none.verdict != SP::MergeVerdict::NoTarget || none.act != act
                || own.verdict != SP::MergeVerdict::Merge || own.target != static_cast<long long>(others.size() - 1) || own.count != 3) {
                ok = false; detail += " every" + std::to_string(act) + "=" + W(none.verdict) + "/" + W(own.verdict);
            }
        }
        // An absent count is one: 1 + 1 = 2.
        expect("absent", SP::DecideMerge(true, LegacyPiece(4, 0), kLegacyBase, { LegacyPiece(4, 0) }), SP::MergeVerdict::Merge, 4, 0, 2);
        // A key-shape piece is not a bag piece, coming in or sitting in the bag.
        expect("key-in", SP::DecideMerge(true, KeyShapePiece(1), kLegacyBase, bag), SP::MergeVerdict::NotPiece, 0, -1, 0);
        expect("key-only", SP::DecideMerge(true, LegacyPiece(1, 1), kLegacyBase, { KeyShapePiece(1) }), SP::MergeVerdict::NoTarget, 1, -1, 0);
        // An unresolved base recognises nothing.
        expect("unresolved", SP::DecideMerge(true, LegacyPiece(1, 1), -1, bag), SP::MergeVerdict::NotPiece, 0, -1, 0);
        check("target/merge-joins-only-a-piece-of-the-same-act", ok, detail);
    }
    {
        // A real item (no reserved seed) is never merged and never a target: the
        // plain logo, another consumable carrying a piece's seed, a key carrying
        // one, a gem, the logo from the unique repository. Beside them, the same
        // bag with one piece of the act added merges into that piece.
        bool ok = true;
        std::string detail;
        const std::vector<SP::BagItem> reals = {
            RealItem(11, 12.0, 4242, 1),                           // the plain logo
            RealItem(11, 12.0, 777001, 1),                         // a signature seed on the logo
            RealItem(11, 13.0, SP::SeedOfAct(1), 1),               // another consumable with a piece's seed
            RealItem(12, 12.0, SP::SeedOfAct(1), 1),               // a key with a piece's seed
            RealItem(15, 12.0, SP::SeedOfAct(1), 1),               // a gem with a piece's seed
            { 11, 12.0, 1.0, SP::SeedOfAct(1), 1 },                // the unique repository
            { 11, 12.5, 0.0, SP::SeedOfAct(1), 1 },                // a fractional logo
        };
        const std::vector<SP::BagItem> pieces = { LegacyPiece(1, 1), LegacyPiece(1, 7) };
        for (size_t i = 0; i < reals.size(); ++i) {
            const SP::MergeDecision in = SP::DecideMerge(true, reals[i], kLegacyBase, pieces);
            if (in.verdict != SP::MergeVerdict::NotPiece || in.target != -1) { ok = false; detail += " in" + std::to_string(i) + "=" + W(in.verdict); }
        }
        const SP::MergeDecision none = SP::DecideMerge(true, LegacyPiece(1, 1), kLegacyBase, reals);
        ok = ok && none.verdict == SP::MergeVerdict::NoTarget && none.target == -1;
        std::vector<SP::BagItem> withPiece = reals;
        withPiece.push_back(LegacyPiece(1, 4));
        const SP::MergeDecision control = SP::DecideMerge(true, LegacyPiece(1, 1), kLegacyBase, withPiece);
        const bool controlOk = control.verdict == SP::MergeVerdict::Merge && control.target == static_cast<long long>(reals.size())
            && control.count == 5;
        check("target/merge-never-takes-a-real-item", ok && controlOk,
            "none=" + W(none.verdict) + " control=" + W(control.verdict) + ":" + std::to_string(control.target) + detail);
    }
    {
        // The game's cap: a merge goes ahead only while the summed count stays
        // at most 999. A full stack is passed over for the next one of the act
        // with room; no stack with room answers `cap`, and an empty bag
        // `no-target`.
        bool ok = SP::kBagStackCap == 999;
        std::string detail;
        struct Case { const char* what; std::vector<long long> counts; long long incoming; SP::MergeVerdict v; long long target; long long count; };
        const Case cases[] = {
            { "998+1", { 998 }, 1, SP::MergeVerdict::Merge, 0, 999 },
            { "999+1", { 999 }, 1, SP::MergeVerdict::Cap, -1, 0 },
            { "990+9", { 990 }, 9, SP::MergeVerdict::Merge, 0, 999 },
            { "990+10", { 990 }, 10, SP::MergeVerdict::Cap, -1, 0 },
            { "full-then-room", { 999, 5 }, 1, SP::MergeVerdict::Merge, 1, 6 },
            { "two-full", { 999, 999 }, 1, SP::MergeVerdict::Cap, -1, 0 },
            { "over-cap", { 1 }, 1000, SP::MergeVerdict::Cap, -1, 0 },
            { "huge", { std::numeric_limits<long long>::max() }, 1, SP::MergeVerdict::Cap, -1, 0 },
            { "empty", {}, 1, SP::MergeVerdict::NoTarget, -1, 0 },
        };
        for (const Case& c : cases) {
            std::vector<SP::BagItem> bag;
            for (long long n : c.counts) bag.push_back(LegacyPiece(5, n));
            const SP::MergeDecision d = SP::DecideMerge(true, LegacyPiece(5, c.incoming), kLegacyBase, bag);
            if (d.verdict != c.v || d.target != c.target || d.count != c.count || d.act != 5) {
                ok = false; detail += std::string(" ") + c.what + "=" + W(d.verdict) + ":" + std::to_string(d.target) + ":" + std::to_string(d.count);
            }
        }
        check("target/merge-respects-the-stack-cap", ok, detail);
    }
    {
        // The counters by verdict and act, and the status line built from them.
        SP::State s;
        bool ok = SP::Apply(s, SP::ParseCommand("1")) && s.on && !s.forced;
        SP::RecordDrop(s, SP::DropVerdict::Hit, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 1);
        SP::RecordDrop(s, SP::DropVerdict::Miss, 2);
        SP::RecordDrop(s, SP::DropVerdict::Town, 1);
        SP::RecordUse(s, SP::UseVerdict::Transfer);
        SP::RecordUse(s, SP::UseVerdict::Transfer);
        SP::RecordUse(s, SP::UseVerdict::BossRoom);
        SP::RecordTransfer(s, 2, true, SP::ZoneAnswer::Yes, SP::ZoneAnswer::No);
        SP::RecordTransfer(s, 1, false, SP::ZoneAnswer::No, SP::ZoneAnswer::No);
        SP::RecordZoneCallFailed(s);
        SP::RecordDropFailed(s);
        const std::string leftSaved = SP::ZoneAnswerText(SP::ZoneAnswer::Yes, "");
        SP::StatusInputs in;
        in.room = "Act_01_01"; in.act = 1; in.gemRate = kMeasuredRate;
        in.gemRoute = "both"; in.useRoute = "both";
        in.left = "Town_01_rm"; in.leftSaved = leftSaved;
        const std::string line = SP::StatusLine(s, in);
        const std::string want = "soulpiece: on force=off room=Act_01_01 act=1 eligible=yes gemrate=9000 chance=1/7200"
            " gemrolls=4 drops=1 misses=2 uses=3 transfers=1 fresh=1 cleared=1 last=Act_02_Boss left=Town_01_rm leftsaved=yes"
            " refused=town=1,drop-failed=1,use-boss-room=1,transfer-failed=1,zone-call-failed=1 hooks=gem:both use:PotionUse:both";
        ok = ok && line == want && s.counters.dropsByAct[1] == 1 && s.counters.transfersByAct[2] == 1
                && s.counters.transfersByAct[1] == 0 && s.counters.transferFailed == 1;
        SP::State off;
        SP::StatusInputs town;
        town.room = "Town_01_rm"; town.act = 1; town.gemRate = 0.0; town.gemRateWhy = "unread";
        const std::string offLine = SP::StatusLine(off, town);
        const std::string wantOff = "soulpiece: off force=off room=Town_01_rm act=1 eligible=no(town) gemrate=unavailable(unread)"
            " chance=1/- gemrolls=0 drops=0 misses=0 uses=0 transfers=0 fresh=0 cleared=0 last=- left=- leftsaved=-"
            " refused=- hooks=gem:none use:PotionUse:none";
        ok = ok && offLine == wantOff && offLine.find("portal") == std::string::npos;
        // An unread answer for the room left names why.
        const std::string unread = SP::ZoneAnswerText(SP::ZoneAnswer::Unread, "call failed");
        SP::StatusInputs left = town;
        left.left = "Act_01_Boss"; left.leftSaved = unread;
        ok = ok && SP::StatusLine(off, left).find(" left=Act_01_Boss leftsaved=unread(call failed) ") != std::string::npos;
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
