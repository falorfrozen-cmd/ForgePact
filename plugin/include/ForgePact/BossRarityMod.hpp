#pragma once

#include "Common.hpp"

namespace ForgePact::BossRarity {

// Bosses (`bossrarity off|rare|ancient|status`, issue #44): every boss the game
// spawns while the mode is on is built as Rare (3, "uber boss") or Ancient
// (4, "uber uber boss").
//
// Where it runs: the shared EnemyRaritySettings hook in ModuleMain.cpp, the
// one the Monster Rarity sliders and Tyrant's Crown already use. The game
// calls EnemyRaritySettings from Enemy_Parent_obj's Alarm 4 for every enemy,
// bosses included, after the spawner has decided `enemyRarity` (1 normal,
// 2 champion, 3 rare, 4 ancient) and before stats, affix effects and the
// health bar are built (live-traced 2026-09-05). Writing the tier there makes
// the game build the boss as if it had rolled that way.
//
// What is raised: an instance whose object descends from
// Enemy_Child_Boss_obj (RarInstanceIsBoss, asked by the hook at the point of
// use), at rarity exactly 1, that no monster created. A boss the game already
// made champion, rare or ancient keeps its own rarity; ordinary monsters are
// never touched by this mode (the sliders keep leaving bosses alone); a boss
// another boss created (a phase, a clone) stays at the game's own rarity, the
// same "enemy-born" rule the sliders follow, so a split child cannot be raised
// again and again.
//
// Affixes: the same top-up the sliders give a raised monster, from the same
// pool (kTyAffixPool, which leaves out the affixes that spawn monsters): up to
// 2 for rare and 3 for ancient, the counts the game's own monsters of that
// tier carry. The owner chose this on 2026-10-02; dropping it is a change to
// the two constants below.
//
// What a forced rarity does to a boss beyond that (health, damage, XP, drops,
// look) is measured, not assumed: docs/boss-rarity-research.md.

enum class Mode : int { Off = 0, Rare = 3, Ancient = 4 };

inline constexpr int kRareTier = 3;
inline constexpr int kAncientTier = 4;
inline constexpr int kRareAffixes = 2;
inline constexpr int kAncientAffixes = 3;

inline const char* ModeName(Mode m)
{
    switch (m) {
    case Mode::Rare: return "rare";
    case Mode::Ancient: return "ancient";
    default: return "off";
    }
}

// The command's words. `0` is off, like every other toggle in the plugin.
// Anything else leaves `out` as it was and answers false.
inline bool ParseMode(std::string_view word, Mode& out)
{
    if (word == "off" || word == "0") { out = Mode::Off; return true; }
    if (word == "rare") { out = Mode::Rare; return true; }
    if (word == "ancient") { out = Mode::Ancient; return true; }
    return false;
}

inline int TierFor(Mode m)
{
    return m == Mode::Rare ? kRareTier : m == Mode::Ancient ? kAncientTier : 0;
}

inline int AffixCountFor(int tier)
{
    return tier == kAncientTier ? kAncientAffixes : tier == kRareTier ? kRareAffixes : 0;
}

// How many affixes the top-up adds to an instance that already carries `have`.
inline int AffixesToAdd(int want, int have)
{
    return want > have ? want - have : 0;
}

// The decision: the tier to write (3 or 4), or 0 to leave the instance alone.
inline int DecideTier(Mode mode, double enemyRarity, bool isBoss, bool enemyBorn)
{
    if (mode == Mode::Off || !isBoss || enemyBorn) return 0;
    return enemyRarity == 1.0 ? TierFor(mode) : 0;
}

// Kept in both builds: `bossrarity status` reports what the mode did, so a
// report can tell "never saw a boss" from "saw them and raised none".
struct Counters {
    std::atomic<long> seen{ 0 };              // bosses the hook judged while the mode was on
    std::atomic<long> raised{ 0 };            // bosses whose rarity was written
    std::atomic<long> raisedRare{ 0 };
    std::atomic<long> raisedAncient{ 0 };
    std::atomic<long> skippedEnemyBorn{ 0 };  // created by a monster: left alone
    std::atomic<long> skippedNotRank1{ 0 };   // already champion/rare/ancient, or unreadable
    std::atomic<long> writeFailed{ 0 };       // the write threw: nothing raised
};

// Off at load: no mod is on by default. Set only by the `bossrarity` command.
inline std::atomic<int> modeValue{ static_cast<int>(Mode::Off) };
inline Counters counters;

inline Mode CurrentMode() { return static_cast<Mode>(modeValue.load(std::memory_order_relaxed)); }
inline bool Active() { return CurrentMode() != Mode::Off; }
inline void SetMode(Mode m) { modeValue.store(static_cast<int>(m), std::memory_order_relaxed); }

// One enemy at the hook's entry. `write(tier, affixes)` sets enemyRarity to
// `tier` and tops the affixes up to `affixes`, answering false when the game
// refused; only a write that went through counts as raised.
template <class Write>
inline int RaiseBoss(Counters& c, Mode m, double enemyRarity, bool isBoss, bool enemyBorn, Write&& write)
{
    if (m == Mode::Off || !isBoss) return 0;
    ++c.seen;
    if (enemyBorn) { ++c.skippedEnemyBorn; return 0; }
    const int tier = DecideTier(m, enemyRarity, isBoss, enemyBorn);
    if (!tier) { ++c.skippedNotRank1; return 0; }
    if (!write(tier, AffixCountFor(tier))) { ++c.writeFailed; return 0; }
    ++c.raised;
    if (tier == kAncientTier) ++c.raisedAncient; else ++c.raisedRare;
    return tier;
}

// `hook` is the shared hook's state as ModuleMain reads it: "ok" (both
// routes), "table-only" (compiled GML's direct calls bypass it), "failed"
// (not installed) or "none" (never asked for).
inline std::string StatusLine(Mode m, const Counters& c, const char* hook)
{
    return std::string("bossrarity: ") + ModeName(m)
        + " raised=" + std::to_string(c.raised.load())
        + " (rare " + std::to_string(c.raisedRare.load()) + ", ancient " + std::to_string(c.raisedAncient.load()) + ")"
        + " seen=" + std::to_string(c.seen.load())
        + " enemyBorn=" + std::to_string(c.skippedEnemyBorn.load())
        + " notRank1=" + std::to_string(c.skippedNotRank1.load())
        + " writeFailed=" + std::to_string(c.writeFailed.load())
        + " hook=" + (hook ? hook : "?");
}

// Whether `bossrarity <mode>` stores the mode it was asked for, given the hook
// state above as read after the command asked for the hook. `off` is always
// stored. `rare` / `ancient` are refused only when the hook failed to install:
// a mode stored then would report itself armed for the whole session while
// raising nothing (the install is attempted once). `table-only` keeps the mode,
// since the table swap still sees table-routed calls, as Tyrant's Crown accepts.
inline bool StoresMode(Mode m, std::string_view hook)
{
    return m == Mode::Off || hook != "failed";
}

// The one line a refused `bossrarity <mode>` answers: the mode asked for, the
// hook state that refused it, and the mode left in place.
inline std::string RefusedLine(Mode asked, Mode kept, const char* hook)
{
    return std::string("bossrarity: refused ") + ModeName(asked)
        + " hook=" + (hook ? hook : "?")
        + " (the shared rarity hook did not install; unchanged: " + ModeName(kept) + ")";
}

} // namespace ForgePact::BossRarity
