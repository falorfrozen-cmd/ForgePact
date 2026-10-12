#pragma once

#include "Common.hpp"

namespace ForgePact::SoulPieces {

// Boss soul pieces (`soulpiece 1|0|on|off|status|force on|force off`, issue
// #197): while the mod is on, an ordinary monster dying in a zone of an act
// can drop a Piece of <boss>'s Soul of that act, and right-clicking a piece in
// the backpack and confirming moves the player straight into that act's boss
// room (`Act_0N_Boss`) through the game's own travel script, after the room's
// remembered state is dropped so the fight is fresh (Live procedure 3's
// goto-travel; the owner chose the direct transfer over a portal, 2026-10-11).
// The boss room's own pentagram brings the boss out; nothing here creates,
// moves or kills a boss.
//
// The drop rolls inside the game's own gem roll (one more consumer of the
// shared `DropGems` detour): a `DropGems` call means the game already decided
// to roll gems for the dying source, so everything that makes it roll gems
// more often (kill density, the monster's own gem chance, a lever on the gem's
// rate) gives more pieces too. The piece adds no gate and no magic-find term
// of its own. Its chance per call is 1 in D, D = max(1, floor(G x 4 / 5)),
// where G is the Gem of Incarnation's `droprate.base`, read at run time from
// its repository entry by name: five quarters of the gem's own chance on the
// same roll, "a little more common" (the owner, 2026-10-11). Live procedure 1
// measured G = 9,000, so D = 7,200.
//
// A piece is built on a key (the follow-up, 2026-10-12): the key base's class
// (12) with `b` = the base's index + act/10 and `c` 0, its seed `a` 777100 +
// act. The bag files it in the Key tab; the game's own stack check compares
// `b` within the runtime's epsilon, so pieces of one act stack, other acts
// (0.1 apart) and the real key (a whole `b`) never join them, with or without
// ForgePact; the repository lookup truncates `b`, so a save holding pieces
// loads them as that key without ForgePact. The pieces the first builds made,
// the game's own Edible PAS Logo (a class-11 consumable, c 0, j 0) with a
// piece's seed, stay recognised as legacy bag pieces: the bag stacks only
// classes 12 to 15, so each takes its own cell. The display sprite is the Gem
// of Incarnation's, which the dress writes under item info key "11".
//
// This header holds the decisions and their counters only: no runtime type,
// builtin or log call. The adapter in ModuleMain.cpp supplies the room's name
// and act (ActOfRoomActIndex of `RoomActIndex`'s answer, asked by name; 0 for
// a room outside the act table), the dying source's ancestry, G, the roll,
// the item's fields and the player's place. tests/soul_piece_harness.cpp runs
// all of it against fake gem rolls and uses; the hub's
// tests/test_soul_piece_model.py pins these constants, the act table and the
// verdict words to hs-game-sdk/curated/boss_soul_pieces.json.

inline constexpr int kActCount = 9;

// A piece's seed is kSeedBase + act (777101..777109). 777001-777003 are the
// signature items' and Miner's Helmet's.
inline constexpr long long kSeedBase = 777100;

// D = floor(G x kRatioNumerator / kRatioDenominator): the piece is
// kRatioDenominator / kRatioNumerator (five quarters) as likely as the gem.
inline constexpr long long kRatioNumerator = 4;
inline constexpr long long kRatioDenominator = 5;
// The Gem of Incarnation's `droprate.base` as Live procedure 1 measured it
// (2026-10-11); the shipped code reads the real one at run time, so a game
// patch or a lever that moves the gem moves the piece with it.
inline constexpr long long kExpectedGemRate = 9000;
// Where G is read: the repository entry of this name in this category,
// walked as `droprate` walks a category, never by index.
inline constexpr int kGemCategory = 15;
inline constexpr char kGemRepoName[] = "socketable_gem_of_incarnation";
// A legacy bag piece's base: the repository entry of this name in this
// category (Live procedure 1 measured b 12 on the owner's own logo; the name is
// resolved at run time, never the index), an item of this class.
inline constexpr int kBaseCategory = 11;
inline constexpr int kBaseItemType = 11;
inline constexpr char kBaseRepoName[] = "consumable_edible_pas_logo";

// A key piece's base: the repository entry of this name in this category
// (#197 Live 3 measured `keys_bifrost_key` at index 2; resolved by name at run
// time, never the index), an item of this class. Which key is the owner's call
// (`## Needs human judgement`, default Bifrost): saved pieces keep the base
// they were made with.
inline constexpr int kKeyBaseCategory = 12;
inline constexpr int kKeyBaseItemType = 12;
inline constexpr char kKeyBaseRepoName[] = "keys_bifrost_key";
// A key piece's `b` is the base's index + act / kActFractionDivisor: tenths,
// because the save writes a real with two decimals (static reading), so a
// tenth survives a reload where a finer step would not.
inline constexpr double kActFractionDivisor = 10.0;
// How far a key piece's `b` (and its `c` from 0) may sit from the exact value
// and still be recognised: wide enough for what a two-decimal save round trip
// returns (at most 0.005 off), well under half the 0.1 between two acts.
inline constexpr double kKeyPieceTolerance = 0.01;

// The display sprite the dress writes under item info key "11" on every piece:
// the Gem of Incarnation's own, resolved by name with `asset_get_index`, never
// an index. The ground item looks up its pickup sprite as `Pickup_` + this name
// (#158's static reading); hs-game-sdk's sprite table holds both. Not saved:
// without ForgePact a piece shows its base's icon.
inline constexpr char kIconInfoKey[] = "11";
inline constexpr char kPieceSpriteName[] = "Gem_Incarnation_spr";
inline constexpr char kGroundSpritePrefix[] = "Pickup_";

// The dress's mechanic tag.
inline constexpr char kMechanicTag[] = "soulpiece";

// ---- the act table ---------------------------------------------------------------
// One row per act: the boss its room is expected to bring out (an SDK object
// name), the piece's name, and the act's boss room (an SDK room name, resolved
// by name at run time), where the piece takes the player. Acts 1 and 2 come
// from the issue; 3-6, 8 and 9 from their boss rooms' decor (static reading);
// Act 7 is the owner's (2026-10-11). `named` false would make an act drop
// nothing and move nobody (`unnamed`); every shipped row is named.
struct ActBoss {
    int act;
    const char* bossObject;
    const char* pieceName;
    const char* bossRoom;
    bool named;
};

inline constexpr ActBoss kActs[kActCount] = {
    { 1, "Gurag_obj", "Piece of Gurag's Soul", "Act_01_Boss", true },
    { 2, "Reaper_obj", "Piece of the Grim Reaper's Soul", "Act_02_Boss", true },
    { 3, "Anubis_obj", "Piece of Anubis' Soul", "Act_03_Boss", true },
    { 4, "Damien_obj", "Piece of Damien's Soul", "Act_04_Boss", true },
    { 5, "Karp_King_obj", "Piece of the Karp King's Soul", "Act_05_Boss", true },
    { 6, "Satan_obj", "Piece of Satan's Soul", "Act_06_Boss", true },
    { 7, "Mevius_obj", "Piece of Mevius' Soul", "Act_07_Boss", true },
    { 8, "Odin_Phase_1_obj", "Piece of Odin's Soul", "Act_08_Boss", true },
    { 9, "Cthulhu_obj", "Piece of Cthulhu's Soul", "Act_09_Boss", true },
};

inline bool IsAct(int act) { return act >= 1 && act <= kActCount; }

inline const ActBoss* BossOfAct(int act)
{
    return IsAct(act) ? &kActs[act - 1] : nullptr;
}

// The act a room belongs to, from `RoomActIndex`'s answer: the answer is the
// act itself (Live procedure 1 read 1.0 in Town_01_rm and Act_01_01 and 2.0 in
// Act_02_01). `isNumber` is false when the answer was not a number at all (the
// script leaves it undefined for any room outside its switch). Anything but a
// whole number from 1 to 9 is no act (0).
inline int ActOfRoomActIndex(bool isNumber, double answer)
{
    if (!isNumber || !(answer >= 1.0 && answer <= static_cast<double>(kActCount))) return 0;
    if (answer != std::floor(answer)) return 0;
    return static_cast<int>(answer);
}

// ---- seeds -------------------------------------------------------------------------
inline long long SeedOfAct(int act) { return IsAct(act) ? kSeedBase + act : 0; }

// The act a seed marks, 0 when it marks none.
inline int ActOfSeed(long long seed)
{
    const long long act = seed - kSeedBase;
    return act >= 1 && act <= kActCount ? static_cast<int>(act) : 0;
}

inline bool IsPiece(long long seed) { return ActOfSeed(seed) != 0; }

// The act of an item that is a legacy bag piece, 0 for anything else: an item
// of the legacy base's class whose definition is the base (`b` the index the
// adapter resolved by name, `c` 0, the normal repository) and whose seed marks
// an act. `baseIndex` below 0 (not resolved yet) recognises nothing.
inline int PieceActOfItem(int itemType, long long b, long long c, long long seed, long long baseIndex)
{
    if (baseIndex < 0 || itemType != kBaseItemType || b != baseIndex || c != 0) return 0;
    return ActOfSeed(seed);
}

// A key piece's `b` for an act: the key base's index + act/10 (index 2: 2.1
// for Act 1 .. 2.9 for Act 9). -1 when the act is none or the base is not
// resolved, so nothing is built.
inline double KeyPieceB(long long baseIndex, int act)
{
    if (baseIndex < 0 || !IsAct(act)) return -1.0;
    return static_cast<double>(baseIndex) + static_cast<double>(act) / kActFractionDivisor;
}

// The act of an item that is a key piece, 0 for anything else. Every signal
// must hold: the key base's class; `c` 0; a `b` that truncates (as the
// game's repository lookup does) to the index the adapter resolved by name,
// that is not whole, and whose tenths are the act its seed marks. The act
// comes from the seed; the fraction only keeps the stacks apart. Each
// comparison allows kKeyPieceTolerance, so a piece read back from a save is
// still a piece. `baseIndex` below 0 (not resolved yet) recognises nothing.
inline int KeyPieceActOfItem(int itemType, double b, double c, long long seed, long long baseIndex)
{
    if (baseIndex < 0 || itemType != kKeyBaseItemType) return 0;
    if (!std::isfinite(b) || !std::isfinite(c) || std::fabs(c) > kKeyPieceTolerance) return 0;
    if (std::trunc(b) != static_cast<double>(baseIndex)) return 0;
    if (b - std::trunc(b) <= kKeyPieceTolerance) return 0;
    const int act = ActOfSeed(seed);
    if (!IsAct(act) || std::fabs(b - KeyPieceB(baseIndex, act)) > kKeyPieceTolerance) return 0;
    return act;
}

// The act of any piece: a key piece on `keyBaseIndex`, or else a legacy bag
// piece on `legacyBaseIndex` (whose `b` and `c` are whole); 0 for anything
// else. Either index below 0 recognises nothing of that shape.
inline int AnyPieceActOfItem(int itemType, double b, double c, long long seed,
    long long keyBaseIndex, long long legacyBaseIndex)
{
    if (const int act = KeyPieceActOfItem(itemType, b, c, seed, keyBaseIndex)) return act;
    if (!std::isfinite(b) || !std::isfinite(c) || b != std::floor(b) || c != std::floor(c)) return 0;
    return PieceActOfItem(itemType, static_cast<long long>(b), static_cast<long long>(c), seed, legacyBaseIndex);
}

// The pickup sprite the ground item looks up for the piece's display sprite.
inline std::string GroundSpriteName() { return std::string(kGroundSpritePrefix) + kPieceSpriteName; }

// ---- verdicts ------------------------------------------------------------------------
// The drop's verdicts, in the order DecideDrop asks them. `force on` overrides
// only the last three (no-rate, hit, miss): never the first seven.
enum class DropVerdict { Off, NoAct, Town, BossRoom, NotMonster, BossKill, Unnamed, NoRate, Hit, Miss, Count };
inline constexpr const char* kDropVerdictWords[] = {
    "off", "no-act", "town", "boss-room", "not-monster", "boss-kill", "unnamed", "no-rate", "hit", "miss",
};
inline constexpr int kDropVerdictCount = static_cast<int>(DropVerdict::Count);
static_assert(sizeof(kDropVerdictWords) / sizeof(kDropVerdictWords[0]) == kDropVerdictCount);

// The use's verdicts, in the order DecideUse asks them; every one but the
// last is a refusal, and the piece is kept.
enum class UseVerdict { Off, NotPiece, NoAct, BossRoom, Unnamed, Transfer, Count };
inline constexpr const char* kUseVerdictWords[] = { "off", "not-piece", "no-act", "boss-room", "unnamed", "transfer" };
inline constexpr int kUseVerdictCount = static_cast<int>(UseVerdict::Count);
static_assert(sizeof(kUseVerdictWords) / sizeof(kUseVerdictWords[0]) == kUseVerdictCount);

inline const char* Word(DropVerdict v)
{
    const int i = static_cast<int>(v);
    return i >= 0 && i < kDropVerdictCount ? kDropVerdictWords[i] : "?";
}

inline const char* Word(UseVerdict v)
{
    const int i = static_cast<int>(v);
    return i >= 0 && i < kUseVerdictCount ? kUseVerdictWords[i] : "?";
}

// ---- rooms -----------------------------------------------------------------------------
// A town drops no piece (a piece may be used in one).
inline bool IsTownRoom(std::string_view room) { return room.starts_with("Town_"); }

// The name rule beside the town: `_Boss` rooms (act boss rooms, boss
// dungeons, Fields_Boss_rm), and the `Uber_` and `Dev_` rooms the game's act
// table also lists. All of them answer `boss-room`, for the drop and the use.
inline bool IsBossRoom(std::string_view room)
{
    return room.find("_Boss") != std::string_view::npos || room.starts_with("Uber_") || room.starts_with("Dev_");
}

// Whether a room drops pieces: it has an act (1..9) and neither name rule
// refuses it. On false, `refusal` holds the word.
inline bool RoomEligible(std::string_view room, int act, DropVerdict& refusal)
{
    if (!IsAct(act)) { refusal = DropVerdict::NoAct; return false; }
    if (IsTownRoom(room)) { refusal = DropVerdict::Town; return false; }
    if (IsBossRoom(room)) { refusal = DropVerdict::BossRoom; return false; }
    return true;
}

// ---- the drop ----------------------------------------------------------------------------
// D for a gem rate G: max(1, floor(G x 4 / 5)); 0 when G is unknown, zero or
// negative (no piece then, unless forced).
inline long long PieceDenominator(double gemRate)
{
    if (!(gemRate > 0.0)) return 0;
    const double d = std::floor(gemRate * static_cast<double>(kRatioNumerator) / static_cast<double>(kRatioDenominator));
    if (d >= 9.0e18) return 9000000000000000000LL;
    return d < 1.0 ? 1 : static_cast<long long>(d);
}

// One `DropGems` call for a dying source. `act` is the room's (0: none);
// `sourceIsMonster`: it descends from Enemy_Parent_obj; `sourceIsBoss`: from
// Enemy_Child_Boss_obj (RarInstanceIsBoss); `gemRate` is G; `roll` is uniform
// in [0, 1).
inline DropVerdict DecideDrop(bool modOn, bool forced, std::string_view room, int act,
    bool sourceIsMonster, bool sourceIsBoss, double gemRate, double roll)
{
    if (!modOn) return DropVerdict::Off;
    DropVerdict refusal = DropVerdict::Off;
    if (!RoomEligible(room, act, refusal)) return refusal;
    if (!sourceIsMonster) return DropVerdict::NotMonster;
    if (sourceIsBoss) return DropVerdict::BossKill;
    const ActBoss* boss = BossOfAct(act);
    if (!boss || !boss->named) return DropVerdict::Unnamed;
    if (forced) return DropVerdict::Hit;
    const long long d = PieceDenominator(gemRate);
    if (d <= 0) return DropVerdict::NoRate;
    return roll < 1.0 / static_cast<double>(d) ? DropVerdict::Hit : DropVerdict::Miss;
}

// ---- the use -------------------------------------------------------------------------------
// A right-click on an item in the backpack, confirmed. `room` and `roomAct`
// are the player's room and its act (0: none); `pieceAct` is the item's
// PieceActOfItem (0: not a piece). Towns are allowed (the owner, after
// Replan 1). A room with no act (uber realms, Chaos Tower, the act boss rooms,
// which `RoomActIndex` leaves undefined) and the drop's boss-room name rule
// refuse it, so a piece is never used inside the room it would reset.
inline UseVerdict DecideUse(bool modOn, std::string_view room, int roomAct, int pieceAct)
{
    if (!modOn) return UseVerdict::Off;
    if (!IsAct(pieceAct)) return UseVerdict::NotPiece;
    if (!IsAct(roomAct)) return UseVerdict::NoAct;
    if (IsBossRoom(room)) return UseVerdict::BossRoom;
    const ActBoss* boss = BossOfAct(pieceAct);
    if (!boss || !boss->named) return UseVerdict::Unnamed;
    return UseVerdict::Transfer;
}

// ---- taking one from a stack ----------------------------------------------------------------
// What the `PotionUse` detour answers for one decided use of a piece whose
// stack holds `count` (the item's `o`), for the route where the game's confirm
// clears the whole stack (Live procedure 1's `stackuse-route: take-one`). A
// refusal answers false with the count unchanged: the piece is kept. A
// transfer from a stack of more than one answers false, so the game keeps the
// cells, with the count one lower (the adapter writes it and refreshes the
// item's hash). A transfer from the last piece (a count of 1, or no count
// above 1) answers true and the game clears the cell.
struct StackTake {
    bool answer;       // what PotionUse answers: true lets the game clear the cell
    long long count;   // the stack's count after the use
};

inline StackTake TakeOne(UseVerdict v, long long count)
{
    if (v != UseVerdict::Transfer) return { false, count };
    if (count > 1) return { false, count - 1 };
    return { true, count };
}

// ---- the transfer --------------------------------------------------------------------------
// Where a piece takes the player: its act's boss room, by SDK room name
// (nullptr for no act). The adapter resolves it with `asset_get_index`, drops
// that room's saved zone state and travels there with `NetworkRoomGoto`.
inline const char* TransferTarget(int pieceAct)
{
    const ActBoss* boss = BossOfAct(pieceAct);
    return boss ? boss->bossRoom : nullptr;
}

// What `ZoneStateExists(<room>)` answered: a saved entry that answers true, no
// entry (or one that answers false), or no answer to read (the call failed, or
// gave neither a number nor a bool). Before the reset it decides `fresh`;
// after it, `cleared`; for the room the player left, the status line's
// `leftsaved`.
enum class ZoneAnswer { Yes, No, Unread };

inline std::string ZoneAnswerText(ZoneAnswer a, std::string_view why)
{
    if (a == ZoneAnswer::Yes) return "yes";
    if (a == ZoneAnswer::No) return "no";
    return "unread(" + std::string(why.empty() ? std::string_view("?") : why) + ")";
}

// ---- the verb ---------------------------------------------------------------------------------
enum class Command { On, Off, Status, ForceOn, ForceOff, Invalid };

// `1|on`, `0|off`, `status` (or nothing), `force on`, `force off`, words
// separated by any run of spaces.
inline Command ParseCommand(std::string_view args)
{
    std::vector<std::string_view> words;
    size_t i = 0;
    while (i < args.size()) {
        while (i < args.size() && args[i] == ' ') ++i;
        const size_t start = i;
        while (i < args.size() && args[i] != ' ') ++i;
        if (i > start) words.push_back(args.substr(start, i - start));
    }
    if (words.empty()) return Command::Status;
    if (words.size() == 1) {
        if (words[0] == "1" || words[0] == "on") return Command::On;
        if (words[0] == "0" || words[0] == "off") return Command::Off;
        if (words[0] == "status") return Command::Status;
        return Command::Invalid;
    }
    if (words.size() == 2 && words[0] == "force") {
        if (words[1] == "on") return Command::ForceOn;
        if (words[1] == "off") return Command::ForceOff;
    }
    return Command::Invalid;
}

// ---- state and counters -----------------------------------------------------------------------
struct Counters {
    long gemRolls = 0;                         // DropGems calls decided
    long drop[kDropVerdictCount]{};            // by verdict
    long dropsByAct[kActCount + 1]{};          // hits, by act
    long dropFailed = 0;                       // hits whose piece could not be placed
    long use[kUseVerdictCount]{};              // piece uses decided, by verdict
    long transfersByAct[kActCount + 1]{};      // transfers carried out, by act
    long transferFailed = 0;                   // `transfer` uses that were not carried out
    long fresh = 0;                            // transfers whose boss room answered a saved state before the reset
    long cleared = 0;                          // of those, the ones whose state was gone after it
    long zoneCallFailed = 0;                   // ZoneStateExists/ZoneStateResetSingle calls that failed on a transfer
};

struct State {
    bool on = false;
    bool forced = false;
    Counters counters;
    int lastTransferAct = 0;                   // 0: none yet
};

// Applies a verb; false for Invalid (nothing changes). `force` is kept apart
// from on/off, and has no effect while off.
inline bool Apply(State& s, Command c)
{
    switch (c) {
    case Command::On: s.on = true; return true;
    case Command::Off: s.on = false; return true;
    case Command::ForceOn: s.forced = true; return true;
    case Command::ForceOff: s.forced = false; return true;
    case Command::Status: return true;
    default: return false;
    }
}

inline void RecordDrop(State& s, DropVerdict v, int act)
{
    const int i = static_cast<int>(v);
    if (i < 0 || i >= kDropVerdictCount) return;
    ++s.counters.gemRolls;
    ++s.counters.drop[i];
    if (v == DropVerdict::Hit && IsAct(act)) ++s.counters.dropsByAct[act];
}

// One decided use of a piece, by its verdict: the detour decides it inside
// `PotionUse`, and the piece is used up only on `transfer`.
inline void RecordUse(State& s, UseVerdict v)
{
    const int i = static_cast<int>(v);
    if (i < 0 || i >= kUseVerdictCount) return;
    ++s.counters.use[i];
}

// A `hit` whose piece the adapter could not build or place (`drop-failed`):
// counted apart, so `drops=` never claims a piece nobody saw.
inline void RecordDropFailed(State& s) { ++s.counters.dropFailed; }

// One `transfer` after the detour decided it. `done`: the travel call went out
// (false: the boss room did not resolve, or by the frame tick the room had
// changed, the player was gone or the call failed; counted `transfer-failed`).
// `before` and `after`: what ZoneStateExists answered for the boss room before
// and after ZoneStateResetSingle. `fresh` counts a carried-out transfer whose
// boss room answered a saved state before the reset; `cleared` those of them
// whose state was gone after it (the entry existed and the reset dropped it).
inline void RecordTransfer(State& s, int act, bool done, ZoneAnswer before, ZoneAnswer after)
{
    if (!done || !IsAct(act)) { ++s.counters.transferFailed; return; }
    ++s.counters.transfersByAct[act];
    if (before == ZoneAnswer::Yes) {
        ++s.counters.fresh;
        if (after == ZoneAnswer::No) ++s.counters.cleared;
    }
    s.lastTransferAct = act;
}

// A ZoneStateExists or ZoneStateResetSingle call on a transfer that the
// runtime reported failed (`zone-call-failed`); the transfer still goes ahead.
inline void RecordZoneCallFailed(State& s) { ++s.counters.zoneCallFailed; }

// ---- the status line ------------------------------------------------------------------------------
// What the adapter knows and the core does not: the room, the gem rate it
// read (or why it could not), each route, and the room the player was in
// before this one with what ZoneStateExists answered for it (`-` for both
// before a room change was seen; `leftSaved` is ZoneAnswerText's word).
struct StatusInputs {
    std::string_view room = "none";
    int act = 0;
    double gemRate = 0.0;
    std::string_view gemRateWhy = "unread";
    std::string_view gemRoute = "none";
    std::string_view useScript = "PotionUse";
    std::string_view useRoute = "none";
    std::string_view left = "-";
    std::string_view leftSaved = "-";
};

inline std::string NumberText(double v)
{
    if (v == std::floor(v) && std::fabs(v) < 9.0e15) return std::to_string(static_cast<long long>(v));
    std::ostringstream out;
    out << v;
    return out.str();
}

// `soulpiece: <on|off> force=<on|off> room=<name> act=<n|none>
// eligible=<yes|no(<word>)> gemrate=<G|unavailable(<why>)> chance=1/<D|->
// gemrolls=<n> drops=<n> misses=<n> uses=<n> transfers=<n> fresh=<n>
// cleared=<n> last=<boss room|-> left=<room|-> leftsaved=<yes|no|unread(<why>)|->
// refused=<word>=<n>,... hooks=gem:<route> use:PotionUse:<route|none>`.
// `uses` counts every piece use decided, refused or not; `transfers` the moves
// carried out; `fresh` the transfers whose boss room answered a saved state
// before the reset, and `cleared` those of them whose state was gone after
// it; `last` the latest transfer's boss room; `left` the room before the
// current one and `leftsaved` its ZoneStateExists answer. `refused` lists the
// drop refusals by their word, `drop-failed`, then the use refusals as
// `use-<word>`, failed transfers as `transfer-failed` and failed zone-state
// calls as `zone-call-failed`, `-` when there is none.
inline std::string StatusLine(const State& s, const StatusInputs& in)
{
    const Counters& c = s.counters;
    DropVerdict refusal = DropVerdict::Off;
    const bool eligible = RoomEligible(in.room, in.act, refusal);
    const long long d = PieceDenominator(in.gemRate);
    std::string refused;
    auto add = [&refused](const std::string& word, long n) {
        if (!n) return;
        if (!refused.empty()) refused += ",";
        refused += word + "=" + std::to_string(n);
    };
    for (int i = 0; i < static_cast<int>(DropVerdict::Hit); ++i) add(kDropVerdictWords[i], c.drop[i]);
    add("drop-failed", c.dropFailed);
    for (int i = 0; i < static_cast<int>(UseVerdict::Transfer); ++i) add(std::string("use-") + kUseVerdictWords[i], c.use[i]);
    add("transfer-failed", c.transferFailed);
    add("zone-call-failed", c.zoneCallFailed);
    long uses = 0;
    for (int i = 0; i < kUseVerdictCount; ++i) uses += c.use[i];
    long transfers = 0;
    for (int act = 1; act <= kActCount; ++act) transfers += c.transfersByAct[act];
    const char* last = TransferTarget(s.lastTransferAct);
    return std::string("soulpiece: ") + (s.on ? "on" : "off")
        + " force=" + (s.forced ? "on" : "off")
        + " room=" + std::string(in.room)
        + " act=" + (IsAct(in.act) ? std::to_string(in.act) : std::string("none"))
        + " eligible=" + (eligible ? std::string("yes") : "no(" + std::string(Word(refusal)) + ")")
        + " gemrate=" + (d > 0 ? NumberText(in.gemRate) : "unavailable(" + std::string(in.gemRateWhy) + ")")
        + " chance=1/" + (d > 0 ? std::to_string(d) : std::string("-"))
        + " gemrolls=" + std::to_string(c.gemRolls)
        + " drops=" + std::to_string(c.drop[static_cast<int>(DropVerdict::Hit)])
        + " misses=" + std::to_string(c.drop[static_cast<int>(DropVerdict::Miss)])
        + " uses=" + std::to_string(uses)
        + " transfers=" + std::to_string(transfers)
        + " fresh=" + std::to_string(c.fresh)
        + " cleared=" + std::to_string(c.cleared)
        + " last=" + (last ? last : "-")
        + " left=" + (in.left.empty() ? std::string("-") : std::string(in.left))
        + " leftsaved=" + (in.leftSaved.empty() ? std::string("-") : std::string(in.leftSaved))
        + " refused=" + (refused.empty() ? std::string("-") : refused)
        + " hooks=gem:" + std::string(in.gemRoute)
        + " use:" + std::string(in.useScript) + ":" + std::string(in.useRoute);
}

} // namespace ForgePact::SoulPieces
