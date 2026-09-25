// Behavioral harness for the Gems of Incarnation core (docs/incarnation-gems-research.md).
//
// The Python runner splices the REAL plugin/include/ForgePact/IncarnationGemsMod.hpp
// in at the marker below (its #pragma and #include lines removed). The header is
// game-independent by contract, so it compiles here with no runtime stub.
//
// Baseline: with the switches off, a fresh drop keeps the seed the game gave it
// and a finished gem keeps its rolls; with them on, anything that is not a Gem
// of Incarnation, or not a fresh drop, is left alone, and a drop whose `n` has
// no seeds yet stays vanilla. Target: a fresh gem drop takes a seed the game
// rolled Mythic at the same `n` - with a filter, one carrying the most wanted
// mods; a finished gem's every affix takes its best tier's range and top value,
// except identifier stats; the tables learn only what the game built and
// survive a restart for the same build only.
//
// Red first: run with INCARNATION_GEMS_BASELINE=1 (the runner then splices a
// stub that never changes anything) and every drop and dress target fails
// while every baseline check passes.
// The header's own includes (the runner strips them from the spliced text).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// PRODUCTION_INCARNATION_GEMS

using namespace ForgePact::IncarnationGems;

static int failures = 0;
static void Check(bool condition, const char* label)
{
    std::cout << (condition ? "PASS " : "FAIL ") << label << "\n";
    if (!condition) ++failures;
}

static Seed S(uint32_t value, std::vector<int> stats) { Seed s; s.value = value; s.stats = std::move(stats); return s; }

// The measured best-tier ranges of five stats (build pe-6aaa6779-0cad4fc8), and
// three Mythic seeds at n 4 with the mods the game gave them.
static Tables Measured()
{
    Tables t;
    t.build = "pe-6aaa6779-0cad4fc8";
    t.best = { { 68, { 3, 12 } }, { 284, { 3, 10 } }, { 57, { 2, 6 } }, { 462, { 2, 433 } }, { 201, { 1, 1 } } };
    t.rows["4"].seeds = { S(11, { 68, 284, 57, 28 }), S(22, { 68, 101, 60, 75 }), S(33, { 150, 146, 52, 53 }) };
    t.rows["none"].seeds = { S(44, { 57, 175, 450, 53, 60 }) };
    return t;
}

static std::vector<Affix> PlagueGod()
{
    // Stat 68 rolled at tier 2 (2-8) as 5, stat 150 unknown to the table.
    return { { 68, 2, 8, 2, 5 }, { 150, 4, 16, 3, 9 } };
}

int main()
{
    // ---- baseline --------------------------------------------------------
    {
        State off;
        const Tables t = Measured();
        Check(!DropSeed(off, t, true, 15, 136, 0, "4", 0), "baseline/off_drop_keeps_seed");
        std::vector<Affix> a = PlagueGod();
        Check(!Dress(off, 15, 136, 0, a, t) && a[0].value == 5 && a[0].tier == 2, "baseline/off_gem_keeps_rolls");
    }
    {
        State on; on.mythic = on.maxRoll = true;
        const Tables t = Measured();
        Check(!DropSeed(on, t, false, 15, 136, 0, "4", 0), "baseline/loaded_gem_keeps_seed");
        Check(!DropSeed(on, t, true, 15, 135, 0, "4", 0), "baseline/other_socketable_keeps_seed");
        Check(!DropSeed(on, t, true, 14, 136, 0, "4", 0), "baseline/other_type_keeps_seed");
        Check(!DropSeed(on, t, true, 15, 136, 1, "4", 0), "baseline/other_kind_keeps_seed");
        Check(!DropSeed(on, t, true, 15, 136, 0, "3", 0), "baseline/n_without_seeds_stays_vanilla");
        std::vector<Affix> a = PlagueGod();
        Check(!Dress(on, 15, 137, 0, a, t) && a[0].value == 5, "baseline/other_item_keeps_rolls");
        State filtered = on; filtered.filter = { 68 };
        Check(!DropSeed(filtered, t, false, 15, 136, 0, "4", 0), "baseline/filter_never_touches_a_loaded_gem");
    }

    // ---- target: drops ---------------------------------------------------
    {
        State on; on.mythic = true;
        const Tables t = Measured();
        const auto seed = DropSeed(on, t, true, 15, 136, 0, "4", 4);
        Check(seed && *seed == 22, "target/drop_takes_mythic_seed_for_its_n");
        const auto none = DropSeed(on, t, true, 15, 136, 0, NKey(std::nullopt), 7);
        Check(none && *none == 44, "target/drop_without_n_uses_its_own_key");
    }
    Check(NKey(std::nullopt) == "none" && NKey(4.0) == "4" && NKey(0.0) == "0", "target/n_keys");
    Check(NKey(std::nan("")) == "none" && NKey(2.5) == "none" && NKey(1e12) == "none", "target/n_keys_refuse_junk");

    // ---- target: the filter picks the seeds carrying the most wanted mods ----
    {
        State on; on.mythic = true;
        const Tables t = Measured();
        on.filter = { 68, 284 };
        bool always11 = true;
        for (uint64_t roll = 0; roll < 12; ++roll) { const auto s = DropSeed(on, t, true, 15, 136, 0, "4", roll); if (!s || *s != 11) always11 = false; }
        Check(always11, "target/filter_prefers_most_wanted_mods");
        on.filter = { 68 };
        std::set<uint32_t> seen;
        for (uint64_t roll = 0; roll < 12; ++roll) seen.insert(*DropSeed(on, t, true, 15, 136, 0, "4", roll));
        Check(seen == std::set<uint32_t>{ 11, 22 }, "target/filter_ties_share_the_drops");
        on.filter = { 999 };
        bool missed = false;
        const auto any = DropSeed(on, t, true, 15, 136, 0, "4", 2, &missed);
        Check(any && missed, "target/filter_without_match_takes_any_mythic_and_says_so");
        on.filter = {};
        missed = true;
        DropSeed(on, t, true, 15, 136, 0, "4", 2, &missed);
        Check(!missed, "target/empty_filter_is_every_mod");
    }
    {
        Check(ParseFilter("all") && ParseFilter("all")->empty(), "target/filter_all");
        const auto two = ParseFilter("68,284");
        Check(two && *two == Filter{ 68, 284 }, "target/filter_list");
        const auto grant = ParseFilter("462,463");
        Check(grant && *grant == Filter{ 462 }, "target/filter_names_a_grant_by_its_skill");
        Check(!ParseFilter("") && !ParseFilter("a,b") && !ParseFilter("68,,284") && !ParseFilter("-1") && !ParseFilter("463"),
              "target/filter_refuses_junk_and_nothing");
        std::string many;
        for (int i = 0; i < 65; ++i) many += (i ? "," : "") + std::to_string(1000 + i);
        Check(!ParseFilter(many), "target/filter_refuses_more_than_64");
    }

    // ---- target: the tables learn only what the game built ----------------
    {
        Tables t;
        t.build = "b";
        t.Plan("4");
        t.Plan("4");
        Check(t.rows.size() == 1 && t.NextWork() == "4", "target/plan_once");
        t.Learn("4", 100, 2, { { 68, 2, 8, 2, 5 } });
        Check(t.rows["4"].tried == 1 && t.rows["4"].seeds.empty() && t.best.empty(), "target/superior_is_counted_not_kept");
        t.Learn("4", 200, 5, { { 68, 3, 12, 4, 7 }, { 284, 2, 6, 2, 3 }, { 462, 2, 433, 4, 117 }, { 463, 1, 1, 4, 1 } });
        Check(t.rows["4"].seeds.size() == 1 && t.rows["4"].seeds[0].value == 200, "target/mythic_seed_kept");
        Check(t.rows["4"].seeds[0].stats == std::vector<int>{ 68, 284, 462 }, "target/mythic_seed_keeps_its_mods_grant_once");
        Check(t.best.size() == 3 && t.best[68] == std::make_pair(3.0, 12.0), "target/best_range_from_tier_4_only");
        t.Learn("4", 200, 5, {});
        Check(t.rows["4"].seeds.size() == 1, "target/seed_kept_once");
        t.Learn("4", 300, 5, { { 68, 1, 99, 4, 50 } });
        Check(t.best[68] == std::make_pair(3.0, 12.0) && t.conflicts == 1, "target/second_range_counted_first_kept");
        for (uint32_t s = 1000; s < 1000 + 2 * kSeedsPerN; ++s) t.Learn("4", s, 5, {});
        Check(t.rows["4"].seeds.size() == kSeedsPerN && t.Complete("4") && t.NextWork().empty(), "target/row_capped_and_complete");
        t.Plan("2");
        Check(t.NextWork() == "2", "target/next_work_fewest_seeds");
    }
    {
        const uint32_t a = Candidate("b1", "4", 0), b = Candidate("b1", "4", 0), c = Candidate("b1", "4", 1),
                       d = Candidate("b1", "3", 0), e = Candidate("b2", "4", 0);
        Check(a == b && a != c && a != d && a != e, "target/candidates_fixed_per_build_n_index");
        bool inRange = true;
        for (uint64_t i = 0; i < 5000; ++i) { const uint32_t s = Candidate("b1", "none", i); if (s < 1 || s > kMaxSeed) inRange = false; }
        Check(inRange, "target/candidates_are_real_seed_range");
        Tables t; t.build = "b1"; t.Plan("4");
        Check(t.NextCandidate("4") == Candidate("b1", "4", 0), "target/next_candidate_follows_tried");
        t.Learn("4", 5, 2, {});
        Check(t.NextCandidate("4") == Candidate("b1", "4", 1), "target/next_candidate_resumes");
    }

    // ---- target: a finished gem at its best --------------------------------
    {
        State on; on.maxRoll = true;
        const Tables t = Measured();
        std::vector<Affix> a = PlagueGod();
        const bool changed = Dress(on, 15, 136, 0, a, t);
        Check(changed && a[0].min == 3 && a[0].max == 12 && a[0].tier == 4 && a[0].value == 12, "target/best_tier_top_value");
        Check(a[1].min == 4 && a[1].max == 16 && a[1].tier == 3 && a[1].value == 16, "target/unknown_best_takes_own_top");
        std::vector<Affix> grant = { { 462, 2, 433, 4, 117 }, { 463, 1, 1, 4, 1 } };
        Dress(on, 15, 136, 0, grant, t);
        Check(grant[0].value == 117 && grant[0].min == 2 && grant[0].max == 433, "target/skill_id_untouched");
        std::vector<Affix> done = { { 68, 3, 12, 4, 12 }, { 201, 1, 1, 4, 1 } };
        Check(!Dress(on, 15, 136, 0, done, t), "target/already_best_is_no_change");
        std::vector<Affix> none;
        Check(!Dress(on, 15, 136, 0, none, t), "target/no_affixes_no_change");
    }

    // ---- target: the tables survive a restart, for their own build only ----
    {
        Tables t = Measured();
        t.rows["4"].tried = 57;
        t.conflicts = 3;
        const std::string text = Serialize(t);
        const Tables back = Parse(text, "pe-6aaa6779-0cad4fc8");
        bool same = back.rows.size() == 2 && back.rows.at("4").tried == 57 && back.rows.at("4").seeds.size() == 3 && back.best == t.best && !back.dirty;
        for (size_t i = 0; same && i < 3; ++i)
            same = back.rows.at("4").seeds[i].value == t.rows.at("4").seeds[i].value && back.rows.at("4").seeds[i].stats == t.rows.at("4").seeds[i].stats;
        Check(same, "target/round_trip_with_mods");
        Check(Parse(text, "pe-other").rows.empty() && Parse(text, "pe-other").build == "pe-other", "target/other_build_starts_empty");
        Check(Parse(text.substr(0, text.size() / 2), "pe-6aaa6779-0cad4fc8").rows.empty(), "target/damaged_file_starts_empty");
        Check(Parse("{\"schema\":1,\"build\":\"x\",\"rows\":{},\"best\":{}}", "x").rows.empty(), "target/older_schema_starts_empty");
        const Tables junk = Parse("{\"schema\":2,\"build\":\"x\",\"rows\":{\"4\":{\"tried\":1,\"seeds\":[[0,68],[5,68],[2.5],[4294967295],[7,28,-1],[9,1,2,3,4,5,6]]}},\"best\":{}}", "x");
        std::vector<uint32_t> kept;
        for (const Seed& s : junk.rows.at("4").seeds) kept.push_back(s.value);
        Check(kept == std::vector<uint32_t>{ 5 }, "target/impossible_seeds_dropped");
    }

    std::cout << (failures ? "RESULT FAILED" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
