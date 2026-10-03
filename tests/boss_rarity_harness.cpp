// Behavioral regression harness for the Bosses control (BossRarityMod.hpp).
//
// The Python runner splices the REAL ForgePact::BossRarity header in below.
// Only the game is replaced: a fake enemy carries the three things the shared
// EnemyRaritySettings hook knows about it when the raise is decided (its
// enemyRarity, whether its object descends from Enemy_Child_Boss_obj, whether
// a monster created it) and its affix count, and the write callback does to
// the fake what ModuleMain's adapter does to the real instance: set
// enemyRarity, then top the affixes up. No game process is touched.
//
// Every "left alone" target runs beside a boss the same mode does raise, so a
// decision that never raises anything fails it instead of passing it.
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

// PRODUCTION_BOSSRARITY

namespace BR = ForgePact::BossRarity;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

// One enemy as the hook sees it at EnemyRaritySettings' entry.
struct Enemy {
    double rarity = 1.0;
    bool boss = false;
    bool enemyBorn = false;
    int affixes = 0;
    int writes = 0;
};

// Runs the production decision on one enemy, writing through a fake that
// mirrors the adapter: enemyRarity first, then the affix top-up.
static int Spawn(BR::Counters& c, BR::Mode mode, Enemy& e) {
    return BR::RaiseBoss(c, mode, e.rarity, e.boss, e.enemyBorn, [&](int tier, int wantAffixes) {
        ++e.writes;
        e.rarity = tier;
        e.affixes += BR::AffixesToAdd(wantAffixes, e.affixes);
        return true;
    });
}
static Enemy Boss(double rarity = 1.0) { Enemy e; e.boss = true; e.rarity = rarity; return e; }
static Enemy Monster(double rarity = 1.0) { Enemy e; e.rarity = rarity; return e; }
static std::string Show(const Enemy& e) {
    return "rarity=" + std::to_string((int)e.rarity) + " affixes=" + std::to_string(e.affixes) + " writes=" + std::to_string(e.writes);
}

int main() {
    // ---- baseline: the mode off is the vanilla game ------------------------
    {
        BR::Counters c;
        Enemy boss = Boss();
        const int tier = Spawn(c, BR::Mode::Off, boss);
        check("baseline/off_boss_untouched",
            tier == 0 && boss.writes == 0 && boss.rarity == 1.0 && c.seen == 0 && c.raised == 0, Show(boss));
    }
    {
        BR::Counters c;
        Enemy m = Monster();
        const int tier = Spawn(c, BR::Mode::Off, m);
        check("baseline/off_normal_monster_untouched",
            tier == 0 && m.writes == 0 && m.rarity == 1.0 && c.seen == 0, Show(m));
    }

    // ---- target: a boss at rarity 1 is built as the chosen tier -------------
    {
        BR::Counters c;
        Enemy boss = Boss();
        const int tier = Spawn(c, BR::Mode::Rare, boss);
        check("target/rare_boss_rank1_to_3",
            tier == 3 && boss.rarity == 3.0 && boss.affixes == 2 && boss.writes == 1 && c.raised == 1 && c.raisedRare == 1,
            Show(boss));
    }
    {
        BR::Counters c;
        Enemy boss = Boss();
        boss.affixes = 1;   // the game's own setup may already have given it one
        const int tier = Spawn(c, BR::Mode::Ancient, boss);
        check("target/ancient_boss_rank1_to_4",
            tier == 4 && boss.rarity == 4.0 && boss.affixes == 3 && boss.writes == 1 && c.raised == 1 && c.raisedAncient == 1,
            Show(boss));
    }

    // ---- target: what the mode leaves alone, each beside a raised control ---
    {
        BR::Counters c;
        Enemy champion = Boss(2.0), rare = Boss(3.0), ancient = Boss(4.0), control = Boss();
        Spawn(c, BR::Mode::Ancient, champion);
        Spawn(c, BR::Mode::Ancient, rare);
        Spawn(c, BR::Mode::Ancient, ancient);
        const int tier = Spawn(c, BR::Mode::Ancient, control);
        const bool untouched = champion.writes + rare.writes + ancient.writes == 0
            && champion.rarity == 2.0 && rare.rarity == 3.0 && ancient.rarity == 4.0;
        check("target/boss_already_rare_untouched",
            untouched && c.skippedNotRank1 == 3 && tier == 4 && control.rarity == 4.0 && c.raised == 1,
            "rare: " + Show(rare) + " control: " + Show(control));
    }
    {
        BR::Counters c;
        Enemy m = Monster(), control = Boss();
        Spawn(c, BR::Mode::Ancient, m);
        const int tier = Spawn(c, BR::Mode::Ancient, control);
        check("target/normal_monster_untouched_in_boss_mode",
            m.writes == 0 && m.rarity == 1.0 && m.affixes == 0 && c.seen == 1 && tier == 4 && control.rarity == 4.0,
            "monster: " + Show(m) + " control: " + Show(control));
    }
    {
        BR::Counters c;
        Enemy born = Boss(), control = Boss();
        born.enemyBorn = true;   // a phase or a clone a boss created itself
        Spawn(c, BR::Mode::Rare, born);
        const int tier = Spawn(c, BR::Mode::Rare, control);
        check("target/enemy_born_boss_untouched",
            born.writes == 0 && born.rarity == 1.0 && c.skippedEnemyBorn == 1 && tier == 3 && control.rarity == 3.0,
            "born: " + Show(born) + " control: " + Show(control));
    }

    // ---- target: the status line says what the mode did ---------------------
    {
        BR::Counters c;
        Enemy a = Boss(), b = Boss(), rare = Boss(3.0), m = Monster();
        Spawn(c, BR::Mode::Ancient, a);
        Spawn(c, BR::Mode::Ancient, b);
        Spawn(c, BR::Mode::Ancient, rare);
        Spawn(c, BR::Mode::Ancient, m);
        const std::string line = BR::StatusLine(BR::Mode::Ancient, c, "ok");
        check("target/status_line_counts_raised",
            line.rfind("bossrarity: ancient ", 0) == 0 && line.find(" raised=2 ") != std::string::npos
                && line.find(" seen=3 ") != std::string::npos && line.find(" notRank1=1 ") != std::string::npos
                && line.find(" hook=ok") != std::string::npos,
            line);
    }

    // ---- the command's words -------------------------------------------------
    {
        BR::Mode m = BR::Mode::Ancient;
        bool ok = BR::ParseMode("off", m) && m == BR::Mode::Off;
        ok = ok && BR::ParseMode("0", m) && m == BR::Mode::Off;
        ok = ok && BR::ParseMode("rare", m) && m == BR::Mode::Rare;
        ok = ok && BR::ParseMode("ancient", m) && m == BR::Mode::Ancient;
        BR::Mode kept = BR::Mode::Rare;
        ok = ok && !BR::ParseMode("uber", kept) && kept == BR::Mode::Rare && !BR::ParseMode("", kept);
        check("command/modes_parsed", ok);
    }
    {
        BR::Counters c;
        Enemy boss = Boss();
        const int tier = BR::RaiseBoss(c, BR::Mode::Rare, boss.rarity, true, false, [&](int, int) { return false; });
        check("command/failed_write_not_counted_raised", tier == 0 && c.raised == 0 && c.writeFailed == 1);
    }
    {
        // What BossRarityCommand stores after it asked for the hook: every
        // mode against every state BossRarityHookState() answers. Only a
        // failed install refuses, and only rare/ancient; the stored mode is
        // then the one already in place. Each refusal sits beside a stored
        // pair, so a decision that refuses everything fails here too.
        const BR::Mode modes[] = { BR::Mode::Off, BR::Mode::Rare, BR::Mode::Ancient };
        const char* states[] = { "ok", "table-only", "failed", "none" };
        bool ok = true;
        std::string detail;
        for (BR::Mode asked : modes) {
            for (const char* hook : states) {
                const bool expect = asked == BR::Mode::Off || std::string_view(hook) != "failed";
                BR::Mode stored = BR::Mode::Off;   // the mode at load
                const bool kept = BR::StoresMode(asked, hook);
                if (kept) stored = asked;
                const bool pair = kept == expect && stored == (expect ? asked : BR::Mode::Off);
                if (!pair) { ok = false; detail += std::string(" ") + BR::ModeName(asked) + "/" + hook; }
            }
        }
        const std::string line = BR::RefusedLine(BR::Mode::Ancient, BR::Mode::Off, "failed");
        ok = ok && line.rfind("bossrarity: refused ancient ", 0) == 0 && line.find(" hook=failed") != std::string::npos
            && line.find("unchanged: off") != std::string::npos;
        check("command/refused_when_hook_failed", ok, line + (detail.empty() ? "" : " wrong:" + detail));
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
