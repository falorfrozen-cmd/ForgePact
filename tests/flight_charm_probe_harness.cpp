// Behavioral regression harness for charmprobe's core
// (FlightCharmProbe.hpp, ForgePact #158 phase 1, research build only).
//
// The Python runner injects the REAL ForgePact::FlightCharms header below. No
// game is touched: a sample is a size and a map of itemInfoStruct numbers, the
// shape the adapter hands the core after GetItemSize and a struct walk.
//
// The size keys: identified from two charms of different sizes, and `unknown`
// from one size only, from a key that matched on one sample but not another,
// from two candidate keys, and from a key standing for both sides. The sprite
// key: identified when one key is a sprite on every sample with two values,
// `unknown` otherwise, and set by hand. The parsers: spawn (sprite included),
// dress, sprite, heroic and repo, accepted and refused forms. The dress rules,
// the Heroic summary and the bare marker line.
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// PRODUCTION_FLIGHTCHARMS

using namespace ForgePact::FlightCharms;

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

static bool spawnOk(const std::string& args, SpawnRequest& r)
{
    std::string why;
    return ParseSpawn(words(args), r, why);
}
static std::string spawnWhy(const std::string& args)
{
    SpawnRequest r;
    std::string why;
    return ParseSpawn(words(args), r, why) ? std::string("ACCEPTED") : why;
}

int main()
{
    // ---- size keys -----------------------------------------------------------
    // The shape a charm's itemInfoStruct gives: the two size keys among other
    // numbers (a rarity, a flag), so a key that only happens to equal w on
    // one sample has to be dropped by the next.
    {
        SizeKeys k;
        const bool a = k.Add(1, 1, { { "w9", 1 }, { "h9", 1 }, { "27", 3 }, { "40", 0 } });
        const bool b = k.Add(1, 2, { { "w9", 1 }, { "h9", 2 }, { "27", 3 }, { "40", 0 } });
        check("size_keys/found_from_two_sizes", a && b && k.Known() && k.W() == "w9" && k.H() == "h9"
              && k.Line() == "w=w9 h=h9" && k.Pair() == "w9,h9", k.Line());
    }
    {
        SizeKeys k;
        k.Add(1, 2, { { "w9", 1 }, { "h9", 2 } });
        k.Add(1, 2, { { "w9", 1 }, { "h9", 2 } });
        check("size_keys/unknown_from_one_size_only", !k.Known() && k.Pair() == "unknown"
              && k.Line() == "unknown (one size only (1x2 in 2 samples))" && k.W().empty(), k.Line());
    }
    {
        SizeKeys k;
        k.Add(1, 1, { { "w9", 1 }, { "h9", 1 } });
        k.Add(1, 2, { { "w9", 1 }, { "h9", 3 } });   // h9 matched h on the first sample only
        check("size_keys/unknown_from_a_key_matching_one_sample_only", !k.Known()
              && k.Line() == "unknown (no key holds h on every sample)", k.Line());
    }
    {
        SizeKeys k;
        k.Add(1, 1, { { "w9", 1 }, { "h9", 1 } });
        k.Add(1, 2, { { "w9", 1 } });                // h9 absent on the second sample
        check("size_keys/unknown_when_the_key_is_missing_on_a_sample", !k.Known()
              && k.Line() == "unknown (no key holds h on every sample)", k.Line());
    }
    {
        SizeKeys k;
        k.Add(1, 1, { { "w9", 1 }, { "h9", 1 }, { "flag", 1 } });
        k.Add(1, 2, { { "w9", 1 }, { "h9", 2 }, { "flag", 1 } });
        check("size_keys/unknown_from_two_candidate_keys", !k.Known()
              && k.Line() == "unknown (2 keys hold w on every sample: flag,w9)", k.Line());
        SizeKeys h;
        h.Add(1, 1, { { "w9", 1 }, { "h9", 1 }, { "h8", 1 } });
        h.Add(2, 2, { { "w9", 2 }, { "h9", 2 }, { "h8", 2 } });
        check("size_keys/unknown_from_three_candidate_keys_on_square_samples", !h.Known()
              && h.Line() == "unknown (3 keys hold w on every sample: h8,h9,w9)", h.Line());
    }
    {
        // The normal charms Live 1 samples (1x1, 1x2, 1x3) all have w = 1, so
        // any other key that is 1 on every one of them is a second w
        // candidate: the rule says unknown rather than pick one.
        SizeKeys k;
        for (int h = 1; h <= 3; ++h) k.Add(1, h, { { "w9", 1 }, { "h9", (double)h }, { "o", 1 } });
        check("size_keys/a_constant_key_equal_to_w_keeps_it_unknown", !k.Known()
              && k.Line() == "unknown (2 keys hold w on every sample: o,w9)", k.Line());
        SizeKeys c;
        for (int h = 1; h <= 3; ++h) c.Add(1, h, { { "w9", 1 }, { "h9", (double)h }, { "o", 7 } });
        check("size_keys/three_sizes_identified_without_one", c.Known() && c.Line() == "w=w9 h=h9", c.Line());
    }
    {
        SizeKeys k;
        k.Add(1, 1, { { "s", 1 } });
        k.Add(2, 2, { { "s", 2 } });
        check("size_keys/one_key_for_both_sides_is_unknown", !k.Known()
              && k.Line() == "unknown (w and h are one key (s))", k.Line());
    }
    {
        SizeKeys k;
        check("size_keys/no_samples", !k.Known() && k.Samples() == 0
              && k.Line() == "unknown (no samples: `charmprobe charms` with charms of two sizes equipped)", k.Line());
        const bool refused = !k.Add(0, 1, { { "w9", 0 } }) && !k.Add(1, 11, { { "w9", 1 } });
        check("size_keys/refuses_a_size_outside_1_to_10", refused && k.Samples() == 0);
        k.Add(1, 1, { { "w9", 1 }, { "h9", 1 } });
        k.Add(1, 2, { { "w9", 1 }, { "h9", 2 } });
        const bool known = k.Known();
        k.Reset();
        check("size_keys/reset_forgets_the_samples", known && !k.Known() && k.Samples() == 0);
    }

    // ---- sprite key ----------------------------------------------------------
    // A sample is the item's numeric keys whose value sprite_exists called a
    // sprite: the item's own icon, plus whatever small number also happens to
    // name a sprite.
    {
        SpriteKey k;
        k.Add({ { "spr", 4501 }, { "27", 3 } });
        k.Add({ { "spr", 4620 } });
        check("sprite_key/found_from_two_samples_with_different_values", k.Known() && !k.ByHand()
              && k.Key() == "spr" && k.Text() == "spr", k.Text());
    }
    {
        SpriteKey k;
        k.Add({ { "spr", 4501 } });
        check("sprite_key/unknown_from_one_sample", !k.Known() && k.Text() == "unknown (one sample only)", k.Text());
        SpriteKey none;
        check("sprite_key/unknown_with_no_samples", !none.Known()
              && none.Text() == "unknown (no samples: `charmprobe charms` or a spawn)", none.Text());
    }
    {
        SpriteKey k;
        k.Add({ { "spr", 4501 } });
        k.Add({ { "spr", 4501 } });
        check("sprite_key/unknown_when_the_value_never_changed", !k.Known()
              && k.Text() == "unknown (spr held one value (4501) on every sample)", k.Text());
    }
    {
        SpriteKey k;
        k.Add({ { "spr", 4501 }, { "alt", 12 } });
        k.Add({ { "spr", 4620 }, { "alt", 13 } });
        check("sprite_key/unknown_from_two_candidate_keys", !k.Known()
              && k.Text() == "unknown (2 keys are sprites on every sample: alt,spr)", k.Text());
    }
    {
        SpriteKey k;
        k.Add({ { "spr", 4501 } });
        k.Add({ { "other", 4620 } });                // spr was not a sprite on this one
        check("sprite_key/unknown_when_no_key_is_a_sprite_on_every_sample", !k.Known()
              && k.Text() == "unknown (no key is a sprite on every sample)", k.Text());
        k.Add({ { "spr", 4700 } });                  // a key once dropped stays dropped
        check("sprite_key/a_dropped_key_stays_dropped", !k.Known());
    }
    {
        SpriteKey k;
        const bool bad = !k.SetByHand("") && !k.SetByHand("a b") && !k.SetByHand(std::string(41, 'x')) && !k.Known();
        const bool good = k.SetByHand("13");
        check("sprite_key/set_by_hand", bad && good && k.Known() && k.ByHand() && k.Key() == "13" && k.Text() == "13", k.Text());
        k.Reset();
        check("sprite_key/reset_forgets_the_hand_key", !k.Known() && !k.ByHand());
    }

    // ---- spawn parser ----------------------------------------------------------
    {
        SpawnRequest r;
        const bool plain = spawnOk("0", r) && r.b == 0 && !r.hasSize && r.stats.empty() && !r.sprite;
        const bool top = spawnOk("59", r) && r.b == 59;
        const bool size = spawnOk("2 size 2 2", r) && r.b == 2 && r.hasSize && r.size.w == 2 && r.size.h == 2;
        const bool tall = spawnOk("2 size 1 5", r) && r.size.w == 1 && r.size.h == 5;
        const bool stat = spawnOk("1 stat 25=100", r) && r.stats.size() == 1 && r.stats[0].first == 25 && r.stats[0].second == 100.0;
        const bool stats = spawnOk("1 stat 25=100 stat 159=-2.5", r) && r.stats.size() == 2 && r.stats[1].first == 159
            && r.stats[1].second == -2.5;
        const bool sprite = spawnOk("3 sprite", r) && r.b == 3 && r.sprite && !r.hasSize;
        const bool all = spawnOk("3 sprite size 2 2 stat 25=10", r) && r.sprite && r.hasSize && r.stats.size() == 1;
        check("parse/spawn_accepted_forms", plain && top && size && tall && stat && stats && sprite && all);
    }
    {
        const std::vector<std::pair<std::string, std::string>> refused = {
            { "", "usage: spawn <b> [size <w> <h>] [stat <id>=<v>]... [sprite]" },
            { "60", "b must be a whole number 0..59 (normal charms), not 60" },
            { "-1", "b must be a whole number 0..59 (normal charms), not -1" },
            { "x", "b must be a whole number 0..59 (normal charms), not x" },
            { "2 size 2", "size needs two whole numbers 1..10" },
            { "2 size 0 2", "size needs two whole numbers 1..10" },
            { "2 size 11 1", "size needs two whole numbers 1..10" },
            { "2 size 2 2 size 1 1", "size given twice" },
            { "1 stat", "stat needs <id>=<value>" },
            { "1 stat 25", "stat needs <id>=<value>, id a whole number 0..999, not 25" },
            { "1 stat =5", "stat needs <id>=<value>, id a whole number 0..999, not =5" },
            { "1 stat 25=x", "stat needs <id>=<value>, id a whole number 0..999, not 25=x" },
            { "1 stat 1000=1", "stat needs <id>=<value>, id a whole number 0..999, not 1000=1" },
            { "1 stat 25=1 stat 25=2", "stat 25 given twice" },
            { "1 stat 1=1 stat 2=1 stat 3=1 stat 4=1 stat 5=1 stat 6=1 stat 7=1 stat 8=1 stat 9=1", "at most 8 stats" },
            { "3 sprite sprite", "sprite given twice" },
            { "3 bogus", "unknown option bogus (size, stat or sprite)" },
        };
        bool ok = true;
        std::string bad;
        for (const auto& [args, why] : refused) {
            const std::string got = spawnWhy(args);
            if (got != why) { ok = false; bad += "[" + args + "] -> " + got + "; "; }
        }
        check("parse/spawn_refused_forms_name_their_reason", ok, bad);
    }
    {
        const bool seeds = SpawnSeed(0) == 779001 && SpawnSeed(4) == 779005;
        const bool key = SpawnKey(1700000000123LL) == "0-0-1700000000123-10";
        const bool json = SpawnJson(779001, 20) == "{\"w\":1,\"a\":779001,\"j\":0,\"b\":20,\"c\":0,\"o\":1}";
        const bool stats = StatsText({}) == "none" && StatsText({ { 25, 100 }, { 159, 2.5 } }) == "25=100,159=2.5";
        check("spawn/seed_key_json_and_stats_text", seeds && key && json && stats);
    }

    // ---- dress -------------------------------------------------------------------
    {
        DressRequest r;
        std::string why;
        const bool show = ParseDress({}, r, why) && r.form == DressForm::Show;
        const bool off = ParseDress({ "off" }, r, why) && r.form == DressForm::Off;
        const bool add = ParseDress({ "779004", "2", "2" }, r, why) && r.form == DressForm::Add && r.rule.a == 779004
            && r.rule.size.w == 2 && r.rule.size.h == 2;
        check("parse/dress_accepted_forms", show && off && add);
        std::string w1, w2, w3, w4;
        const bool r1 = !ParseDress({ "779004", "2" }, r, w1) && w1 == "usage: dress <a> <w> <h> | dress off";
        const bool r2 = !ParseDress({ "x", "2", "2" }, r, w2) && w2 == "a must be a whole number (the item definition's seed), not x";
        const bool r3 = !ParseDress({ "779004", "0", "2" }, r, w3) && w3 == "w and h must be whole numbers 1..10";
        const bool r4 = !ParseDress({ "on" }, r, w4) && w4 == "usage: dress <a> <w> <h> | dress off";
        check("parse/dress_refused_forms_name_their_reason", r1 && r2 && r3 && r4, w1 + "|" + w2 + "|" + w3 + "|" + w4);
    }
    {
        DressRules d;
        std::string why;
        bool added = true;
        for (long long a = 1; a <= 8; ++a) added = d.Add({ a, { 1, 1 } }, why) && added;
        const bool ninth = !d.Add({ 9, { 2, 2 } }, why) && why == "8 rules already (`dress off` clears them)";
        const bool replaced = d.Add({ 3, { 2, 2 } }, why) && d.Count() == 8 && d.Find(3)->size.w == 2;
        check("dress/at_most_8_rules_and_one_per_a", added && ninth && replaced, why);
        const bool find = d.Find(4) != nullptr && d.Find(10) == nullptr && d.Find(4.5) == nullptr
            && d.Find(std::nan("")) == nullptr;
        check("dress/only_a_whole_a_matches", find);
        DressRules e;
        e.Add({ 779004, { 2, 2 } }, why);
        e.NoteDressed();
        e.NoteDressed();
        const bool line = e.Line() == "rules=1 [779004:2x2] dressed=2";
        e.Clear();
        check("dress/line_and_off", line && e.Empty() && e.Line() == "rules=0 [none] dressed=2", e.Line());
    }

    // ---- sprite parser -------------------------------------------------------------
    {
        SpriteRequest r;
        std::string why;
        const bool show = ParseSprite({}, r, why) && r.form == SpriteForm::Show;
        const bool load = ParseSprite({ "load" }, r, why) && r.form == SpriteForm::Load;
        const bool key = ParseSprite({ "key", "13" }, r, why) && r.form == SpriteForm::Key && r.key == "13";
        check("parse/sprite_accepted_forms", show && load && key);
        std::string w1, w2, w3;
        const bool r1 = !ParseSprite({ "key" }, r, w1) && w1 == "sprite key needs one key name: 1..40 letters, digits or _";
        const bool r2 = !ParseSprite({ "key", "a;b" }, r, w2) && w2 == w1;
        const bool r3 = !ParseSprite({ "unload" }, r, w3) && w3 == "usage: sprite | sprite load | sprite key <name>";
        check("parse/sprite_refused_forms_name_their_reason", r1 && r2 && r3, w1 + "|" + w2 + "|" + w3);
    }

    // ---- heroic and repo -------------------------------------------------------------
    {
        int n = 0;
        std::string why;
        const bool one = ParseHeroic({ "1" }, n, why) && n == 1;
        const bool twenty = ParseHeroic({ "20" }, n, why) && n == 20;
        const std::string usage = "usage: heroic <n>, n a whole number 1..20";
        std::string w1, w2, w3, w4;
        const bool refused = !ParseHeroic({}, n, w1) && !ParseHeroic({ "0" }, n, w2) && !ParseHeroic({ "21" }, n, w3)
            && !ParseHeroic({ "5", "5" }, n, w4) && w1 == usage && w2 == usage && w3 == usage && w4 == usage;
        check("parse/heroic_1_to_20", one && twenty && refused);
        RepoRequest r;
        const bool repo = ParseRepo({ "10", "0", "76" }, r, why) && r.type == 10 && r.sub == 0 && r.b == 76;
        std::string w5;
        const bool repoBad = !ParseRepo({ "10", "0" }, r, w5) && !ParseRepo({ "10", "x", "76" }, r, w5)
            && w5 == "usage: repo <t> <sub> <b>, whole numbers (t, sub 0..99, b 0..9999)";
        check("parse/repo_forms", repo && repoBad, w5);
    }
    {
        HeroicTally t;
        check("heroic/summary_with_nothing_placed", t.Line() == "calls=0 placed=0 rarity=none charms=0", t.Line());
        t.Add(true, true, 9, true);
        t.Add(true, true, 4, false);
        t.Add(false, false, 0, false);
        t.Add(true, true, 9, false);
        t.Add(true, false, 0, false);
        check("heroic/summary_line", t.Calls() == 5 && t.Placed() == 4
              && t.Line() == "calls=5 placed=4 rarity=4:1,9:2,?:1 charms=1", t.Line());
    }

    // ---- text and the marker ---------------------------------------------------------
    {
        const bool numbers = NumberText(3) == "3" && NumberText(-2) == "-2" && NumberText(2.5) == "2.5"
            && NumberText(std::nan("")) == "?";
        const bool dump = InfoDump({ { "28", "Small Charm" }, { "100", "1" }, { "3", "2" }, { "name", "x" } })
            == "3=2;28=Small Charm;100=1;name=x" && InfoDump({}) == "none";
        const bool fits = InfoTextFits(std::string(40, 'a')) && !InfoTextFits(std::string(41, 'a'));
        check("text/numbers_and_info_dump", numbers && dump && fits);
        SizeKeys k;
        DressRules d;
        std::string why;
        const bool before = MarkerLine(k, d) == "charmprobe: research build marker=fcp1 sizekeys=unknown dress=0";
        k.Add(1, 1, { { "w9", 1 }, { "h9", 1 } });
        k.Add(1, 2, { { "w9", 1 }, { "h9", 2 } });
        d.Add({ 779004, { 2, 2 } }, why);
        const bool after = MarkerLine(k, d) == "charmprobe: research build marker=fcp1 sizekeys=w9,h9 dress=1";
        check("marker/line", before && after, MarkerLine(k, d));
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
