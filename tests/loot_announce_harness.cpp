// Behavioral regression harness for the Loot announcements mod's decision
// core (LootAnnounceMod.hpp, `lootann 1|0|stat`, ForgePact #17).
//
// The Python runner injects the REAL ForgePact::LootAnnounceMod header below.
// No game is touched: a ground item is the three things the adapter in
// ModuleMain.cpp reads off it and hands the core - the rarity code from
// itemInfoStruct["27"] (or kRarityUnread when that read failed), the ground
// instance id and the item's itemTimeStamp - plus whether the item arrived
// while the LootGroundDrop detour's bag-drop window was open.
//
// Baseline: with the switch off, a Heroic item (and every other rarity) is
// not announced, nothing is counted and nothing is remembered. Target: with
// it on, Heroic, Angelic and Unholy announce once; Satanic, Mythic, Common
// and the rest do not, nor does an unreadable rarity; a second sight of the
// same item (same ground id and time stamp) does not; a bag drop does not;
// the memory survives off/on and is capped; the counters and the stat line
// say what happened.
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// PRODUCTION_LOOT_ANNOUNCE_MOD

using ForgePact::LootAnnounceMod;
using Verdict = LootAnnounceMod::Verdict;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string counts(const LootAnnounceMod& m)
{
    const auto& c = m.Stats();
    return "seen=" + std::to_string(c.seen) + " announced=" + std::to_string(c.announced)
        + " held-rarity=" + std::to_string(c.heldRarity) + " held-no-rarity=" + std::to_string(c.heldNoRarity)
        + " held-duplicate=" + std::to_string(c.heldDuplicate) + " held-bag-drop=" + std::to_string(c.heldBagDrop)
        + " sink-refused=" + std::to_string(c.sinkRefused) + " remembered=" + std::to_string(m.Remembered());
}

// One ground item as the adapter sees it at the end of the drop's frame.
struct Drop {
    int rarity;
    int64_t id;
    std::string stamp;
};

static Verdict see(LootAnnounceMod& m, const Drop& d, bool bagDrop = false)
{
    return m.Decide(d.rarity, bagDrop, d.id, d.stamp);
}

// The rarity codes of RUNTIME_DATA_MODELS.md section 16.4 (4 and 8 were
// never observed; they are offered too, so "not listed" is tested, not
// assumed).
enum Rarity : int { Common = 1, Superior = 2, Rare = 3, Code4 = 4, Mythic = 5, Satanic = 6, Angelic = 7,
                    Code8 = 8, Heroic = 9, Unholy = 10 };

int main()
{
    // ---- the table: what is announced, named --------------------------------
    {
        const std::set<int> set(LootAnnounceMod::kAnnouncedRarities.begin(), LootAnnounceMod::kAnnouncedRarities.end());
        check("table/announced_rarities_are_heroic_angelic_unholy",
              set == std::set<int>{ 9, 7, 10 } && LootAnnounceMod::kAnnouncedRarities.size() == 3
                  && LootAnnounceMod::kHeroic == 9 && LootAnnounceMod::kAngelic == 7 && LootAnnounceMod::kUnholy == 10);
        bool exact = true;
        for (int code = -2; code <= 12; ++code) {
            const bool want = code == 9 || code == 7 || code == 10;
            if (LootAnnounceMod::IsAnnouncedRarity(code) != want) exact = false;
        }
        check("table/is_announced_rarity_exact", exact);
        check("table/unread_is_not_a_rarity", !LootAnnounceMod::IsAnnouncedRarity(LootAnnounceMod::kRarityUnread)
                                                  && LootAnnounceMod::kRarityUnread < 0);
        check("table/shipped_sink_and_names",
              LootAnnounceMod::kShippedSink == LootAnnounceMod::Sink::Server
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::Method)) == "method"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::NetSend)) == "netsend"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::ChatAdd)) == "chatadd"
                  && std::string(LootAnnounceMod::SinkName(LootAnnounceMod::Sink::Server)) == "server",
              LootAnnounceMod::SinkName(LootAnnounceMod::kShippedSink));
    }

    // ---- baseline: the switch off --------------------------------------------
    {
        LootAnnounceMod m;
        check("baseline/starts_off", !m.Enabled() && m.StatusLine() == "lootann: off", m.StatusLine());
        const Verdict v = see(m, { Heroic, 100, "1700000000001" });
        check("baseline/off_heroic_not_announced", v == Verdict::Off, LootAnnounceMod::VerdictName(v));
        bool allOff = true;
        for (int code : std::vector<int>{ Common, Superior, Rare, Mythic, Satanic, Angelic, Heroic, Unholy,
                                          LootAnnounceMod::kRarityUnread })
            if (see(m, { code, 200 + code, "s" + std::to_string(code) }) != Verdict::Off) allOff = false;
        m.BeginBagDrop();
        if (see(m, { Heroic, 300, "bag" }, m.BagDropActive()) != Verdict::Off) allOff = false;
        m.EndBagDrop();
        check("baseline/off_every_rarity_not_announced", allOff);
        check("baseline/off_counts_and_remembers_nothing",
              m.Stats().seen == 0 && m.Stats().announced == 0 && m.Remembered() == 0, counts(m));
        // Switched on later, the item seen while off is new to it.
        m.SetEnabled(true);
        check("baseline/item_seen_while_off_is_new_when_on", see(m, { Heroic, 100, "1700000000001" }) == Verdict::Announce,
              counts(m));
    }

    // ---- target: Heroic, Angelic and Unholy announce once --------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        check("target/on_status", m.Enabled() && m.StatusLine() == "lootann: on", m.StatusLine());
        const Verdict h = see(m, { Heroic, 1001, "t1001" });
        const Verdict a = see(m, { Angelic, 1002, "t1002" });
        const Verdict u = see(m, { Unholy, 1003, "t1003" });
        check("target/heroic_announced", h == Verdict::Announce, LootAnnounceMod::VerdictName(h));
        check("target/angelic_announced", a == Verdict::Announce, LootAnnounceMod::VerdictName(a));
        check("target/unholy_announced", u == Verdict::Announce, LootAnnounceMod::VerdictName(u));
        check("target/three_announced_counted", m.Stats().announced == 3 && m.Stats().seen == 3 && m.Remembered() == 3,
              counts(m));
    }

    // ---- target: everything below Heroic, and an unread rarity, stays quiet --
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Verdict s = see(m, { Satanic, 2001, "t2001" });
        const Verdict y = see(m, { Mythic, 2002, "t2002" });
        const Verdict c = see(m, { Common, 2003, "t2003" });
        check("target/satanic_not_announced", s == Verdict::HeldRarity, LootAnnounceMod::VerdictName(s));
        check("target/mythic_not_announced", y == Verdict::HeldRarity, LootAnnounceMod::VerdictName(y));
        check("target/common_not_announced", c == Verdict::HeldRarity, LootAnnounceMod::VerdictName(c));
        bool rest = true;
        for (int code : std::vector<int>{ Superior, Rare, Code4, Code8, 0, 11, 16 })
            if (see(m, { code, 2100 + code, "r" + std::to_string(code) }) != Verdict::HeldRarity) rest = false;
        check("target/other_codes_not_announced", rest, counts(m));
        const Verdict n = see(m, { LootAnnounceMod::kRarityUnread, 2004, "t2004" });
        check("target/unread_rarity_not_announced_and_counted",
              n == Verdict::HeldNoRarity && m.Stats().heldNoRarity == 1, counts(m));
        check("target/held_counted_never_announced_never_remembered",
              m.Stats().announced == 0 && m.Stats().heldRarity == 10 && m.Remembered() == 0 && m.Stats().seen == 11,
              counts(m));
    }

    // ---- target: a second sight of the same item -----------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const Drop item{ Heroic, 3001, "1759600000123" };
        const Verdict first = see(m, item);
        const Verdict second = see(m, item);
        const Verdict third = see(m, item);
        check("target/second_sight_not_announced",
              first == Verdict::Announce && second == Verdict::HeldDuplicate && third == Verdict::HeldDuplicate
                  && m.Stats().announced == 1 && m.Stats().heldDuplicate == 2,
              counts(m));
        // The key is the pair: another ground instance, or another item in the
        // same instance, is a different sight.
        const Verdict otherId = see(m, { Heroic, 3002, "1759600000123" });
        const Verdict otherStamp = see(m, { Heroic, 3001, "1759600000999" });
        check("target/key_is_ground_id_and_time_stamp",
              otherId == Verdict::Announce && otherStamp == Verdict::Announce && m.Stats().announced == 3, counts(m));
        // Off, then on again: the memory is kept, so a still-lying item is not
        // announced a second time.
        m.SetEnabled(false);
        m.SetEnabled(true);
        check("target/memory_kept_across_off_on", see(m, item) == Verdict::HeldDuplicate, counts(m));
    }

    // ---- target: a bag drop --------------------------------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        check("target/bag_drop_window_closed_at_start", !m.BagDropActive());
        {
            LootAnnounceMod::BagDropScope scope(m);
            check("target/bag_drop_window_open_in_scope", m.BagDropActive());
            const Verdict v = see(m, { Heroic, 4001, "t4001" }, m.BagDropActive());
            check("target/bag_drop_not_announced", v == Verdict::HeldBagDrop && m.Stats().heldBagDrop == 1,
                  LootAnnounceMod::VerdictName(v) + std::string(" ") + counts(m));
            {
                LootAnnounceMod::BagDropScope nested(m);
            }
            check("target/bag_drop_window_nests", m.BagDropActive());
        }
        check("target/bag_drop_window_closes", !m.BagDropActive());
        // A bag drop is not remembered: the game dropping a fresh Heroic after
        // it is announced.
        check("target/bag_drop_not_remembered", m.Remembered() == 0 && m.Stats().announced == 0, counts(m));
        check("target/after_bag_drop_a_game_drop_announces", see(m, { Heroic, 4002, "t4002" }) == Verdict::Announce,
              counts(m));
        // An unbalanced end never underflows the window.
        m.EndBagDrop();
        m.EndBagDrop();
        check("target/bag_drop_window_never_negative", !m.BagDropActive()
            && see(m, { Unholy, 4003, "t4003" }) == Verdict::Announce, counts(m));
    }

    // ---- the memory is capped ------------------------------------------------
    {
        LootAnnounceMod m;
        m.SetEnabled(true);
        const int64_t total = (int64_t)LootAnnounceMod::kMemoryCap + 1;
        long announced = 0;
        for (int64_t i = 0; i < total; ++i)
            if (see(m, { Angelic, 10000 + i, "c" + std::to_string(i) }) == Verdict::Announce) ++announced;
        check("memory/capped", m.Remembered() == LootAnnounceMod::kMemoryCap && announced == total, counts(m));
        // The oldest is forgotten first; the newest is still remembered.
        check("memory/oldest_forgotten_first",
              see(m, { Angelic, 10000 + total - 1, "c" + std::to_string(total - 1) }) == Verdict::HeldDuplicate
                  && see(m, { Angelic, 10000, "c0" }) == Verdict::Announce,
              counts(m));
    }

    // ---- the sink's refusal and the stat line --------------------------------
    {
        LootAnnounceMod m;
        check("stat/fresh_off",
              m.StatLine() == "lootann: off route=server seen=0 announced=0 held-rarity=0 held-no-rarity=0 "
                              "held-duplicate=0 held-bag-drop=0 sink-refused=0 remembered=0",
              m.StatLine());
        m.SetEnabled(true);
        see(m, { Heroic, 1, "a" });
        see(m, { Heroic, 1, "a" });
        see(m, { Satanic, 2, "b" });
        see(m, { LootAnnounceMod::kRarityUnread, 3, "c" });
        m.BeginBagDrop();
        see(m, { Angelic, 4, "d" }, m.BagDropActive());
        m.EndBagDrop();
        m.NoteSinkRefused();
        check("stat/counts",
              m.StatLine() == "lootann: on route=server seen=5 announced=1 held-rarity=1 held-no-rarity=1 "
                              "held-duplicate=1 held-bag-drop=1 sink-refused=1 remembered=1",
              m.StatLine());
        check("stat/verdict_names",
              std::string(LootAnnounceMod::VerdictName(Verdict::Off)) == "off"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::Announce)) == "announce"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldRarity)) == "held-rarity"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldNoRarity)) == "held-no-rarity"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldDuplicate)) == "held-duplicate"
                  && std::string(LootAnnounceMod::VerdictName(Verdict::HeldBagDrop)) == "held-bag-drop");
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
