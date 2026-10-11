#pragma once

#include "Common.hpp"

namespace ForgePact::SoulPieces {

// Boss soul pieces (`soulpiece 1|0|on|off|status|force on|force off`, issue
// #197): while the mod is on, an ordinary monster dying in a zone of an act
// can drop a Piece of <boss>'s Soul of that act, and right-clicking a piece in
// the backpack outside town summons that act's boss near the player.
//
// The drop rolls inside the game's own gem roll (one more consumer of the
// shared `DropGems` detour): a `DropGems` call means the game already decided
// to roll gems for the dying source, so everything that makes it roll gems
// more often (kill density, the monster's own gem chance, a lever on the gem's
// rate) gives more pieces too. The piece adds no gate and no magic-find term
// of its own. Its chance per call is 1 in D, D = max(1, floor(G x 4 / 5)),
// where G is the Gem of Incarnation's `droprate.base`, read at run time from
// its repository entry by name: five quarters of the gem's own chance on the
// same roll, "a little more common" (the owner, 2026-10-11). G = 10,000 is the
// expected rate, UNVERIFIED until Live procedure 1 reads it.
//
// A piece is one of the game's own key definitions (class 12, a base key
// resolved by repository name, c 0, j 0) whose seed `a` is 777100 + act, so a
// save holding pieces loads without ForgePact. The game stacks two items when
// their class, b and c agree and never compares the seed, so while the mod is
// on VetoStack keeps pieces of different acts, and a piece and an ordinary
// key, apart (only on `item-route: one-base`).
//
// This header holds the decisions and their counters only: no runtime type,
// builtin or log call. The adapter in ModuleMain.cpp supplies the room's name
// and act (`RoomActIndex` + 1, asked by name; 0 for a room outside the act
// table), the dying source's ancestry, G, the roll and the items' seeds.
// tests/soul_piece_harness.cpp runs all of it against fake gem rolls and
// uses; the hub's tests/test_soul_piece_model.py pins these constants, the act
// table and the verdict words to hs-game-sdk/curated/boss_soul_pieces.json.

inline constexpr int kActCount = 9;

// A piece's seed is kSeedBase + act (777101..777109). 777001-777003 are the
// signature items' and Miner's Helmet's.
inline constexpr long long kSeedBase = 777100;

// D = floor(G x kRatioNumerator / kRatioDenominator): the piece is
// kRatioDenominator / kRatioNumerator (five quarters) as likely as the gem.
inline constexpr long long kRatioNumerator = 4;
inline constexpr long long kRatioDenominator = 5;
// The Gem of Incarnation's expected `droprate.base` (UNVERIFIED, static
// reading; the shipped code reads the real one at run time).
inline constexpr long long kExpectedGemRate = 10000;
// Where G is read: the repository entry of this name in this category,
// walked as `droprate` walks a category, never by index.
inline constexpr int kGemCategory = 15;
inline constexpr char kGemRepoName[] = "socketable_gem_of_incarnation";
// Pieces are keys: their base is an entry of this category, found by name.
inline constexpr int kKeyCategory = 12;

// The summoned boss appears this far to the player's side.
inline constexpr double kSpawnOffsetPx = 600.0;
// The dress's mechanic tag.
inline constexpr char kMechanicTag[] = "soulpiece";

// ---- the act table ---------------------------------------------------------------
// One row per act: the boss summoned for it (an SDK object name, resolved by
// name at run time) and the piece's name. Acts 1 and 2 come from the issue;
// 3-6, 8 and 9 from their boss rooms' decor (static reading); Act 7 is the
// owner's (2026-10-11). `named` false would make an act drop nothing and
// summon nothing (`unnamed`); every shipped row is named.
struct ActBoss {
    int act;
    const char* bossObject;
    const char* pieceName;
    bool named;
};

inline constexpr ActBoss kActs[kActCount] = {
    { 1, "Gurag_obj", "Piece of Gurag's Soul", true },
    { 2, "Reaper_obj", "Piece of the Grim Reaper's Soul", true },
    { 3, "Anubis_obj", "Piece of Anubis' Soul", true },
    { 4, "Damien_obj", "Piece of Damien's Soul", true },
    { 5, "Karp_King_obj", "Piece of the Karp King's Soul", true },
    { 6, "Satan_obj", "Piece of Satan's Soul", true },
    { 7, "Mevius_obj", "Piece of Mevius' Soul", true },
    { 8, "Odin_Phase_1_obj", "Piece of Odin's Soul", true },
    { 9, "Cthulhu_obj", "Piece of Cthulhu's Soul", true },
};

inline bool IsAct(int act) { return act >= 1 && act <= kActCount; }

inline const ActBoss* BossOfAct(int act)
{
    return IsAct(act) ? &kActs[act - 1] : nullptr;
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

// ---- verdicts ------------------------------------------------------------------------
// The drop's verdicts, in the order DecideDrop asks them. `force on` overrides
// only the last three (no-rate, hit, miss): never the first seven.
enum class DropVerdict { Off, NoAct, Town, BossRoom, NotMonster, BossKill, Unnamed, NoRate, Hit, Miss, Count };
inline constexpr const char* kDropVerdictWords[] = {
    "off", "no-act", "town", "boss-room", "not-monster", "boss-kill", "unnamed", "no-rate", "hit", "miss",
};
inline constexpr int kDropVerdictCount = static_cast<int>(DropVerdict::Count);
static_assert(sizeof(kDropVerdictWords) / sizeof(kDropVerdictWords[0]) == kDropVerdictCount);

// The use's verdicts, in the order DecideUse asks them.
enum class UseVerdict { Off, Town, NotPiece, Unnamed, Summon, Count };
inline constexpr const char* kUseVerdictWords[] = { "off", "town", "not-piece", "unnamed", "summon" };
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
// A town removes a boss spawned in it, and neither drops nor summons.
inline bool IsTownRoom(std::string_view room) { return room.starts_with("Town_"); }

// The name rule beside the town: `_Boss` rooms (act boss rooms, boss
// dungeons, Fields_Boss_rm), and the `Uber_` and `Dev_` rooms the game's act
// table also lists. All of them answer `boss-room`.
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
// A right-click on an item in the backpack. `pieceAct` is ActOfSeed of the
// item's seed (0: not a piece). A use is allowed in any room but a town.
inline UseVerdict DecideUse(bool modOn, std::string_view room, int pieceAct)
{
    if (!modOn) return UseVerdict::Off;
    if (IsTownRoom(room)) return UseVerdict::Town;
    if (!IsAct(pieceAct)) return UseVerdict::NotPiece;
    const ActBoss* boss = BossOfAct(pieceAct);
    if (!boss || !boss->named) return UseVerdict::Unnamed;
    return UseVerdict::Summon;
}

// ---- the stack veto ------------------------------------------------------------------------
// Consulted only after the game answered that two items stack. True turns
// that answer into false: exactly one of them is a piece, or both are pieces
// of different acts. Off, the game's answer stands.
inline bool VetoStack(bool modOn, long long seedA, long long seedB)
{
    if (!modOn) return false;
    const int a = ActOfSeed(seedA);
    const int b = ActOfSeed(seedB);
    return a != b;
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
    long use[kUseVerdictCount]{};              // by verdict
    long summonsByAct[kActCount + 1]{};        // bosses that appeared, by act
    long spawnFailed = 0;                      // `summon` whose spawn returned no instance
};

struct State {
    bool on = false;
    bool forced = false;
    Counters counters;
    int lastSummonedAct = 0;                   // 0: none yet
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

// `spawned`: for `summon`, whether the spawn returned an instance (the piece
// is taken only then).
inline void RecordUse(State& s, UseVerdict v, int act, bool spawned)
{
    const int i = static_cast<int>(v);
    if (i < 0 || i >= kUseVerdictCount) return;
    if (v == UseVerdict::Summon && !spawned) { ++s.counters.spawnFailed; return; }
    ++s.counters.use[i];
    if (v == UseVerdict::Summon && IsAct(act)) {
        ++s.counters.summonsByAct[act];
        s.lastSummonedAct = act;
    }
}

// ---- held pieces --------------------------------------------------------------------------------
// The Key tab's pieces, as the adapter reads them: count and stacks per act.
struct Held {
    long count[kActCount + 1]{};
    long stacks[kActCount + 1]{};
};

inline void AddHeld(Held& h, long long seed, long stackCount)
{
    const int act = ActOfSeed(seed);
    if (!act) return;
    h.count[act] += stackCount;
    ++h.stacks[act];
}

// `<act>:<count>/<stacks>,...`, or `-` when no piece is held.
inline std::string HeldText(const Held& h)
{
    std::string out;
    for (int act = 1; act <= kActCount; ++act) {
        if (!h.stacks[act]) continue;
        if (!out.empty()) out += ",";
        out += std::to_string(act) + ":" + std::to_string(h.count[act]) + "/" + std::to_string(h.stacks[act]);
    }
    return out.empty() ? "-" : out;
}

// ---- the status line ------------------------------------------------------------------------------
// What the adapter knows and the core does not: the room, the gem rate it
// read (or why it could not), the held pieces and each detour's route.
struct StatusInputs {
    std::string_view room = "none";
    int act = 0;
    double gemRate = 0.0;
    std::string_view gemRateWhy = "unread";
    std::string held = "-";
    std::string_view gemRoute = "none";
    std::string_view useScript = "none";
    std::string_view useRoute = "none";
    std::string_view stackRoute = "none";
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
// gemrolls=<n> drops=<n> misses=<n> uses=<n> summoned=<object|->
// refused=<word>=<n>,... held=<act>:<count>/<stacks>,...
// hooks=gem:<route> use:<script>:<route> stack:<route|none>`.
// `refused` lists the drop refusals by their word, then the use refusals as
// `use-<word>` and failed spawns as `spawn-failed`, `-` when there is none.
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
    for (int i = 0; i < static_cast<int>(UseVerdict::Summon); ++i) add(std::string("use-") + kUseVerdictWords[i], c.use[i]);
    add("spawn-failed", c.spawnFailed);
    const ActBoss* summoned = BossOfAct(s.lastSummonedAct);
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
        + " uses=" + std::to_string(c.use[static_cast<int>(UseVerdict::Summon)])
        + " summoned=" + (summoned ? summoned->bossObject : "-")
        + " refused=" + (refused.empty() ? std::string("-") : refused)
        + " held=" + in.held
        + " hooks=gem:" + std::string(in.gemRoute)
        + " use:" + std::string(in.useScript) + ":" + std::string(in.useRoute)
        + " stack:" + std::string(in.stackRoute);
}

} // namespace ForgePact::SoulPieces
