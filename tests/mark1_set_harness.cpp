// Behavioral regression harness for the Mark 1 set's core
// (Mark1Set.hpp, ForgePact #166 phase 2).
//
// The Python runner injects the REAL ForgePact::Mark1Set header below. No game
// is touched: the worn gear slots 0-9 are a WornSlots array the scenarios
// fill, handed to the core through a read callback that counts its calls, so a
// baseline can assert that nothing was read.
//
// Baseline (labels baseline/*): a vanilla item, one piece worn twice in place
// of another, three pieces, an unreadable slot, a failed read and the switch
// off all leave the bonus inactive; with the switch off nothing is read at all.
// Seed codec (seed_codec/*): the table's classes, seeds and bases, the
// seeds distinct and inside ForgePact's built-in band without touching a band
// in use, and recognition (`b` and `c` ignored, `j` must be 0, a whole
// number). Targets: the four pieces worn, each in whichever gear slot holds
// it (Mod live 2's `equip-detect`: the gloves were not at slot 3), with the
// switch on make the bonus active, and the line names each piece's slot
// (active/*); the install state's transitions (install/*); the
// refresh period (refresh/*); the dressing (dressing/*), and its tier and
// rarity: an item that is not a piece keeps both, every piece comes out SS and
// Satanic (tier_baseline/*, tier_target/*, rarity_baseline/*,
// rarity_target/*; the owner, 2026-10-20); the stat line
// (stat/*) and the command's forms (parse/*). failure_counts/*: every field
// the line prints is there once, in order, and each failure count reaches it
// under its own name. art_rule/*: the pieces' art is armed by the first
// piece dressed in a session (baseline: a fresh one writes and installs
// nothing), and the write and the ground install follow the rule. drops_arm/*:
// the `drops 1|0` form, and a drops switch-on arms the same art flag without
// touching the set's switch (phase 3, Mark1Drops.hpp). art_baseline/* and
// art_target/*: `mark1set bonus 1` alone no longer arms the art; a piece's
// dressing and the drops' switch do, both sticky; the pieces' type line goes
// in once armed, embedded art or not. tooltip_baseline/* and
// tooltip_target/*: the set block a piece's tooltip draws, its rows and
// colours by what is worn, the switch and the read, and the piece's type line
// (the owner's playtest, 2026-10-21).
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// PRODUCTION_MARK1SET

using namespace ForgePact::Mark1Set;

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

// One worn slot holding the item a row describes, with any b and c.
static WornSlot item(double type, double a, double b = 0, double c = 0, double j = 0)
{
    WornSlot s;
    s.read = SlotRead::Item;
    s.itemType = type;
    s.a = a;
    s.b = b;
    s.c = c;
    s.j = j;
    return s;
}

static WornSlot pieceIn(Piece p)
{
    const PieceRow& r = RowOf(p);
    return item(r.itemType, static_cast<double>(r.legacySeed), r.baseB, 0, 0);
}

static WornSlot emptySlot()
{
    WornSlot s;
    s.read = SlotRead::Empty;
    return s;
}

// The four pieces in gear slots 0-3, in table order, and the other gear slots
// empty.
static WornSlots fullSet()
{
    WornSlots w;
    for (auto& s : w) s = emptySlot();
    for (int i = 0; i < kPieceCount; ++i) w[static_cast<size_t>(i)] = pieceIn(kPieces[i].piece);
    return w;
}

// The adapter's worn read, as a callback: hands back `slots`, or fails as a
// whole when `ok` is false. Counts its calls.
struct FakeWorn {
    WornSlots slots = fullSet();
    bool ok = true;
    int calls = 0;
    Bonus::ReadWornFn fn()
    {
        return [this](WornSlots& out) {
            ++calls;
            if (!ok) return false;
            out = slots;
            return true;
        };
    }
};

// A Bonus with its switch on and its hooks in.
static Bonus installedOn()
{
    Bonus b;
    b.SetEnabled(true);
    b.NoteInstall(true, false);
    return b;
}

static bool startsWith(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }
static bool has(const std::string& s, const std::string& p) { return s.find(p) != std::string::npos; }

// The stat line's `name=value` words: the names in order, and one value.
static std::vector<std::string> fieldNames(const std::string& line)
{
    std::vector<std::string> out;
    for (const std::string& w : words(line)) {
        const size_t eq = w.find('=');
        if (eq != std::string::npos) out.push_back(w.substr(0, eq));
    }
    return out;
}

static std::string field(const std::string& line, const std::string& name)
{
    for (const std::string& w : words(line))
        if (w.rfind(name + "=", 0) == 0) return w.substr(name.size() + 1);
    return "<missing>";
}

static std::string why(const std::string& args)
{
    Command c;
    std::string reason;
    return ParseCommand(words(args), c, reason) ? std::string("ACCEPTED") : reason;
}

int main()
{
    // ---- the table, printed for the Python side to pin against the SDK -------
    for (const PieceRow& r : kPieces)
        std::cout << "TABLE " << r.key << " type=" << r.itemType << " seed=" << r.legacySeed
                  << " b=" << r.baseB << std::endl;

    // ---- seed_codec --------------------------------------------------------------
    {
        bool ok = kPieceCount == 4 && kGearSlotCount == 10
            && kPieces[0].piece == Piece::Helmet && kPieces[0].itemType == 0
            && kPieces[1].piece == Piece::Body && kPieces[1].itemType == 1
            && kPieces[2].piece == Piece::Boots && kPieces[2].itemType == 2
            && kPieces[3].piece == Piece::Gloves && kPieces[3].itemType == 4;
        check("seed_codec/classes_in_piece_order_and_ten_gear_slots", ok);

        ok = kPieces[0].legacySeed == 777500 && kPieces[1].legacySeed == 777501 && kPieces[2].legacySeed == 777502 && kPieces[3].legacySeed == 777503;
        check("seed_codec/one_seed_per_piece", ok);

        ok = true;
        for (int i = 0; i < kPieceCount; ++i) {
            if (kPieces[i].legacySeed < kBuiltInSeedLo || kPieces[i].legacySeed > kBuiltInSeedHi) ok = false;
            for (int k = i + 1; k < kPieceCount; ++k) if (kPieces[i].legacySeed == kPieces[k].legacySeed) ok = false;
        }
        check("seed_codec/seeds_distinct_inside_the_built_in_band", ok && kBuiltInSeedLo == 777000 && kBuiltInSeedHi == 999 + 777000);

        // The built-in bands in use when the table was written (the workorder's
        // context, § "The pieces and the full-set read"): the signature, soul,
        // relic and flight ranges, and the procedural band from 779000 up.
        struct Band { long long lo, hi; };
        const Band inUse[] = { { 777001, 777003 }, { 777100, 777109 }, { 777200, 777343 }, { 777400, 777490 },
                               { 779000, std::numeric_limits<long long>::max() } };
        ok = true;
        for (const PieceRow& r : kPieces)
            for (const Band& b : inUse)
                if (r.legacySeed >= b.lo && r.legacySeed <= b.hi) ok = false;
        check("seed_codec/seeds_collide_with_no_band_in_use", ok);

        ok = kPieces[0].baseB == 7 && kPieces[1].baseB == 0 && kPieces[2].baseB == 0 && kPieces[3].baseB == 0;
        check("seed_codec/measured_bases", ok);

        ok = true;
        for (const PieceRow& r : kPieces) {
            Piece p = Piece::Gloves;
            if (!IsPiece(r.itemType, static_cast<double>(r.legacySeed), r.baseB, 0, 0, p) || p != r.piece) ok = false;
        }
        check("seed_codec/each_piece_recognised", ok);

        Piece p = Piece::Helmet;
        ok = IsPiece(2, 777502, 0, 0, 0, p) && p == Piece::Boots
            && IsPiece(2, 777502, 31, 5, 0, p) && p == Piece::Boots
            && IsPiece(2, 777502, -1, 999, 0, p) && p == Piece::Boots
            && IsPiece(0, 777500, 0, 3, 0, p) && p == Piece::Helmet;
        check("seed_codec/b_and_c_do_not_matter", ok);

        ok = !IsPiece(2, 777502, 0, 0, 1, p) && !IsPiece(2, 777502, 0, 0, 12, p) && !IsPiece(2, 777502, 0, 0, 0.5, p);
        check("seed_codec/j_must_be_zero", ok);

        ok = !IsPiece(0, 777502, 0, 0, 0, p)      // the boots' seed on a helmet
            && !IsPiece(4, 777500, 0, 0, 0, p)    // the helmet's seed on gloves
            && !IsPiece(3, 777500, 0, 0, 0, p)    // a weapon
            && !IsPiece(1, 777504, 0, 0, 0, p)    // the seed after the table
            && !IsPiece(1, 777499, 0, 0, 0, p)    // the seed before it
            && !IsPiece(1, 777501.5, 0, 0, 0, p)  // not a whole number
            && !IsPiece(1.5, 777501, 0, 0, 0, p)
            && !IsPiece(std::nan(""), 777501, 0, 0, 0, p)
            && !IsPiece(1, std::numeric_limits<double>::infinity(), 0, 0, 0, p);
        check("seed_codec/other_classes_seeds_and_reals_refused", ok);

        Piece k = Piece::Helmet;
        ok = PieceFromKey("helmet", k) && k == Piece::Helmet && PieceFromKey("body", k) && k == Piece::Body
            && PieceFromKey("boots", k) && k == Piece::Boots && PieceFromKey("gloves", k) && k == Piece::Gloves
            && !PieceFromKey("set", k) && !PieceFromKey("Helmet", k) && !PieceFromKey("", k);
        check("seed_codec/keys_round_trip", ok);
    }

    // ---- dressing ------------------------------------------------------------------
    {
        bool ok = kPieces[0].name == "Mark 1 Helmet" && kPieces[1].name == "Mark 1 Body Armor"
            && kPieces[2].name == "Mark 1 Boots" && kPieces[3].name == "Mark 1 Gloves";
        check("dressing/display_names", ok);
        check("dressing/satanic_rarity_and_mechanic", kRarity == 6 && kMechanic == "mark1");
        // No affix rows: the set block's name row reads exactly "Mark 1".
        check("dressing/the_set_name_reads_mark_1", kSetName == "Mark 1");
        // The constants as compiled, for the runner to pin against hs-game-sdk's
        // curated item_info.json (`tiers` SS, `rarities` Satanic).
        std::cout << "DRESSING tier=" << kTier << " rarity=" << kRarity << std::endl;
    }

    // ---- the dressing's tier and rarity (owner, 2026-10-20) ------------------------
    {
        // What an item came with, read off its info before dressing: every tier
        // code, and -1 for one the base left unset.
        const double tiers[] = { -1, 1, 2, 3, 4, 5 };
        const double rarities[] = { -1, 1, 2, 3, 5, 6, 7, 9, 10 };

        // Baseline: an item that is not a piece keeps the tier it came with. A
        // vanilla helmet, a weapon on a piece's seed, the seed after the table,
        // a unique on a piece's seed (j 1) and a stand-in-shaped unique.
        struct Other { double t, a, b, c, j; const char* what; };
        const Other others[] = {
            { 0, 1234, 7, 0, 0, "vanilla-helmet" }, { 3, 777500, 0, 0, 0, "weapon-on-a-seed" },
            { 1, 777504, 0, 0, 0, "seed-after-the-table" }, { 2, 777502, 0, 1, 1, "unique-on-a-seed" },
            { 4, 4321, 3, 1, 12, "stand-in" },
        };
        bool ok = true;
        std::string bad;
        for (const Other& o : others)
            for (double t : tiers) {
                const Dressed d = Dress(o.t, o.a, o.b, o.c, o.j, 6, t);
                if (d.tier != t) { ok = false; bad += std::string(o.what) + ":" + std::to_string(t) + " "; }
            }
        check("tier_baseline/an_item_that_is_not_a_piece_keeps_its_tier", ok, bad);

        // Baseline: a piece's info before dressing holds the base's value,
        // whatever it is; the decision reads it and changes nothing it was given.
        ok = true;
        for (const PieceRow& r : kPieces)
            for (double t : tiers) {
                double before = t;
                (void)Dress(r.itemType, static_cast<double>(r.legacySeed), r.baseB, 0, 0, 1, before);
                if (before != t) ok = false;
            }
        check("tier_baseline/a_pieces_info_before_dressing_holds_the_bases_value", ok);

        // Target: every piece, all four classes, comes out of the dressing at
        // tier 5 (SS), whatever tier its base gave it, with any b and c.
        ok = true;
        bad.clear();
        for (const PieceRow& r : kPieces)
            for (double t : tiers)
                for (double b : { static_cast<double>(r.baseB), 0.0, 12.0 })
                    for (double c : { 0.0, 1.0 }) {
                        const Dressed d = Dress(r.itemType, static_cast<double>(r.legacySeed), b, c, 0, 9, t);
                        if (d.tier != 5 || d.tier != kTier) { ok = false; bad += std::string(r.key) + " "; }
                    }
        check("tier_target/every_piece_comes_out_at_tier_5", ok, bad);

        // Rarity baseline: an item that is not a piece keeps the rarity it came
        // with, and a stand-in's build before the rewrite (a unique of the
        // piece's class, a different seed, j not 0) carries the stand-in's own.
        ok = true;
        bad.clear();
        for (const Other& o : others)
            for (double rr : rarities) {
                const Dressed d = Dress(o.t, o.a, o.b, o.c, o.j, rr, 3);
                if (d.rarity != rr || d.tier != 3) { ok = false; bad += std::string(o.what) + ":" + std::to_string(rr) + " "; }
            }
        check("rarity_baseline/an_item_that_is_not_a_piece_keeps_its_rarity", ok, bad);
        ok = true;
        for (const PieceRow& r : kPieces)
            for (double rr : { 9.0, 6.0, 3.0 }) {
                const Dressed d = Dress(r.itemType, 4321, 3, 1, 12, rr, 4);   // the stand-in of this class
                if (d.rarity != rr) ok = false;
            }
        check("rarity_baseline/a_stand_ins_build_before_the_rewrite_keeps_its_own_rarity", ok);

        // Rarity target: every piece, all four classes, comes out at 6
        // (Satanic), whatever rarity its base gave it; never 9 (Heroic).
        ok = true;
        bad.clear();
        for (const PieceRow& r : kPieces)
            for (double rr : rarities) {
                const Dressed d = Dress(r.itemType, static_cast<double>(r.legacySeed), r.baseB, 0, 0, rr, -1);
                if (d.rarity != 6 || d.rarity != kRarity || d.rarity == 9) { ok = false; bad += std::string(r.key) + " "; }
            }
        check("rarity_target/every_piece_comes_out_at_rarity_6", ok, bad);
    }

    // ---- the full-set read -------------------------------------------------------
    {
        SetRead r = Evaluate(fullSet());
        check("active/four_matching_pieces_are_full", r.full && r.matched == 4 && r.unreadable == 0 && SetText(r) == "full",
              SetText(r));
        check("active/worn_names_each_pieces_slot", WornText(r) == "0/1/2/3", WornText(r));

        // Mod live 2's `equip-detect`: the gloves worn in a gear slot other
        // than 3 (slot 3 empty) still count, and the line says where.
        WornSlots w = fullSet();
        w[3] = emptySlot();
        w[5] = pieceIn(Piece::Gloves);
        r = Evaluate(w);
        check("active/gloves_in_another_gear_slot_are_full", r.full && r.matched == 4 && SetText(r) == "full"
                  && r.foundSlot[3] == 5 && WornText(r) == "0/1/2/5", SetText(r) + " worn=" + WornText(r));

        // Any order across slots 0-9 is the same set.
        w = fullSet();
        for (auto& s : w) s = emptySlot();
        w[9] = pieceIn(Piece::Helmet);
        w[4] = pieceIn(Piece::Body);
        w[0] = pieceIn(Piece::Boots);
        w[7] = pieceIn(Piece::Gloves);
        r = Evaluate(w);
        check("active/pieces_in_any_gear_slots_are_full", r.full && WornText(r) == "9/4/0/7", WornText(r));

        // A piece worn twice counts once, at the first slot that holds it.
        w = fullSet();
        w[6] = pieceIn(Piece::Boots);
        r = Evaluate(w);
        check("active/a_piece_twice_counts_once_at_its_first_slot", r.full && r.matched == 4 && WornText(r) == "0/1/2/3",
              WornText(r));

        // An unreadable slot cannot hide a piece already found: the other
        // gear (weapons, rings) never blocks a full set, and it is counted.
        w = fullSet();
        w[8].read = SlotRead::Unreadable;
        r = Evaluate(w);
        check("active/an_unreadable_other_slot_beside_four_pieces_is_full_and_counted", r.full && r.unreadable == 1,
              SetText(r));

        // Other gear whose definition has no `a` or `j` (the adapter hands
        // NaN): read, not a piece, and not unreadable.
        w = fullSet();
        w[8] = item(4, std::nan(""), 0, 0, 0);
        w[3] = item(4, 777503, 0, 0, std::nan(""));
        r = Evaluate(w);
        check("baseline/gear_without_a_or_j_is_read_and_not_a_piece", !r.full && r.matched == 3 && r.unreadable == 0
                  && WornText(r) == "0/1/2/-", SetText(r) + " worn=" + WornText(r));

        w = fullSet();
        w[2] = item(2, 123456, 0, 0, 0);
        r = Evaluate(w);
        check("baseline/a_vanilla_item_is_not_full", !r.full && r.matched == 3 && SetText(r) == "partial(3)", SetText(r));

        w = fullSet();
        w[3] = emptySlot();
        r = Evaluate(w);
        check("baseline/three_pieces_are_not_full", !r.full && r.matched == 3 && r.unreadable == 0 && SetText(r) == "partial(3)",
              SetText(r));

        // A second helmet worn where the body armor was: the helmet counts
        // once and the body armor is missing.
        w = fullSet();
        w[1] = pieceIn(Piece::Helmet);
        r = Evaluate(w);
        check("baseline/a_piece_twice_in_place_of_another_does_not_count", !r.full && r.matched == 3 && SetText(r) == "partial(3)"
                  && WornText(r) == "0/-/2/3", SetText(r) + " worn=" + WornText(r));

        // Helmets in every gear slot: one counts.
        WornSlots helmets;
        for (auto& s : helmets) s = pieceIn(Piece::Helmet);
        r = Evaluate(helmets);
        check("baseline/the_same_piece_in_every_slot_is_one", !r.full && r.matched == 1 && SetText(r) == "partial(1)", SetText(r));

        w = fullSet();
        w[1].read = SlotRead::Unreadable;
        r = Evaluate(w);
        check("baseline/an_unreadable_slot_is_not_full", !r.full && r.matched == 3 && r.unreadable == 1 && WornText(r) == "0/-/2/3",
              SetText(r));

        WornSlots none;
        for (auto& s : none) s = emptySlot();
        r = Evaluate(none);
        check("baseline/no_piece_reads_none", !r.full && r.matched == 0 && SetText(r) == "none" && WornText(r) == "-/-/-/-",
              SetText(r));
    }

    // ---- the bonus at the point of use ---------------------------------------------
    {
        Bonus b;
        FakeWorn worn;
        bool active = b.Active(0, worn.fn());
        check("baseline/switch_off_reads_nothing_and_is_inactive", !active && worn.calls == 0 && b.Reads() == 0);

        b.NoteInstall(true, false);
        active = b.Active(100, worn.fn());
        check("baseline/switch_off_installed_still_inactive", !active && worn.calls == 0);

        Bonus on = installedOn();
        FakeWorn full;
        active = on.Active(0, full.fn());
        check("active/four_pieces_switch_on_is_active", active && full.calls == 1 && on.Reads() == 1 && on.ActiveCached());

        Bonus three = installedOn();
        FakeWorn w3;
        w3.slots[0] = emptySlot();
        check("baseline/three_pieces_switch_on_inactive", !three.Active(0, w3.fn()) && three.LastRead().matched == 3);

        Bonus vanilla = installedOn();
        FakeWorn wv;
        wv.slots[1] = item(1, 42, 0, 0, 0);
        check("baseline/a_vanilla_item_switch_on_inactive", !vanilla.Active(0, wv.fn()));

        Bonus twice = installedOn();
        FakeWorn ww;
        ww.slots[3] = pieceIn(Piece::Boots);
        check("baseline/a_piece_twice_in_place_of_another_switch_on_inactive", !twice.Active(0, ww.fn()));

        // The target: the gloves away from slot 3 still make it active.
        Bonus moved = installedOn();
        FakeWorn wm;
        wm.slots[3] = emptySlot();
        wm.slots[7] = pieceIn(Piece::Gloves);
        check("active/gloves_in_another_gear_slot_switch_on_is_active", moved.Active(0, wm.fn()) && moved.ActiveCached());

        Bonus unread = installedOn();
        FakeWorn wu;
        wu.slots[3].read = SlotRead::Unreadable;
        active = unread.Active(0, wu.fn());
        check("baseline/an_unreadable_slot_switch_on_inactive_and_counted", !active && unread.Unreadable() == 1);

        Bonus failed = installedOn();
        FakeWorn wf;
        wf.ok = false;
        active = failed.Active(0, wf.fn());
        check("baseline/a_failed_read_is_inactive_and_counted",
              !active && failed.Unreadable() == 1 && failed.LastRead().matched == 0 && SetText(failed.LastRead()) == "none");

        // An empty callback (the adapter could not build one) is a failed read.
        Bonus nofn = installedOn();
        active = nofn.Active(0, Bonus::ReadWornFn{});
        check("baseline/no_read_callback_is_a_failed_read", !active && nofn.Unreadable() == 1);

        // Turning the switch off ends the bonus at once and forgets the read.
        Bonus off = installedOn();
        FakeWorn wo;
        off.Active(0, wo.fn());
        off.SetEnabled(false);
        active = off.Active(1, wo.fn());
        check("baseline/switch_turned_off_ends_the_bonus_at_once", !active && !off.ActiveCached() && wo.calls == 1);
    }

    // ---- the refresh period ------------------------------------------------------
    {
        check("refresh/period_is_30_frames", Bonus::kRefreshFrames == 30);

        Bonus b = installedOn();
        FakeWorn w;
        for (unsigned long long f = 0; f < 30; ++f) b.Active(f, w.fn());
        check("refresh/at_most_one_read_in_30_frames", w.calls == 1, "calls=" + std::to_string(w.calls));
        b.Active(30, w.fn());
        check("refresh/reads_again_on_frame_30", w.calls == 2, "calls=" + std::to_string(w.calls));

        // Many calls in one frame read once.
        Bonus same = installedOn();
        FakeWorn ws;
        for (int i = 0; i < 10; ++i) same.Active(500, ws.fn());
        check("refresh/many_calls_one_frame_one_read", ws.calls == 1);

        // A piece taken off is noticed at the next read, not before.
        Bonus t = installedOn();
        FakeWorn wt;
        const bool before = t.Active(0, wt.fn());
        wt.slots[2] = emptySlot();
        const bool cached = t.Active(29, wt.fn());
        const bool after = t.Active(30, wt.fn());
        check("refresh/a_piece_taken_off_ends_it_at_the_next_read", before && cached && !after);

        // A counter that went backwards (a new session) reads at once.
        Bonus back = installedOn();
        FakeWorn wb;
        back.Active(1000, wb.fn());
        back.Active(5, wb.fn());
        check("refresh/a_frame_counter_that_went_back_reads_at_once", wb.calls == 2);

        // Turning the switch on again reads at once, whatever the period says.
        Bonus re = installedOn();
        FakeWorn wr;
        re.Active(0, wr.fn());
        re.SetEnabled(false);
        re.SetEnabled(true);
        re.Active(1, wr.fn());
        check("refresh/switch_on_again_reads_at_once", wr.calls == 2);

        // `mark1set stat` reads now, whatever the period says, and the point of
        // use counts its period from that read.
        Bonus st = installedOn();
        FakeWorn wst;
        st.Active(0, wst.fn());
        st.ReadNow(10, wst.fn());
        st.Active(35, wst.fn());
        const int afterStat = wst.calls;
        st.Active(40, wst.fn());
        check("refresh/stat_reads_now", afterStat == 2 && wst.calls == 3, "calls=" + std::to_string(wst.calls));

        // stat with the switch off still reads (the line shows the set), and
        // the bonus stays inactive.
        Bonus so;
        FakeWorn wso;
        so.ReadNow(0, wso.fn());
        check("refresh/stat_reads_with_the_switch_off_and_stays_inactive",
              wso.calls == 1 && so.LastRead().full && !so.ActiveCached());
    }

    // ---- the install state ---------------------------------------------------------
    {
        Bonus b;
        check("install/fresh_is_not_armed", b.InstallStateName() == "not-armed" && !b.TickNeeded() && !b.Installed());
        check("install/switch_off_never_installs", !b.ShouldInstall(true, true, false) && !b.ShouldInstall(true, true, true));

        b.SetEnabled(true);
        check("install/switch_on_without_a_character_waits",
              b.InstallStateName() == "waiting-for-character" && !b.ShouldInstall(true, false, false) && b.TickNeeded());
        check("install/not_before_setup", !b.ShouldInstall(false, true, true));

        check("install/tick_looks_every_60_frames", Bonus::kInstallPollFrames == 60 && Bonus::LooksForPlayer(0)
              && !Bonus::LooksForPlayer(1) && !Bonus::LooksForPlayer(59) && Bonus::LooksForPlayer(60) && Bonus::LooksForPlayer(120));

        check("install/tick_installs_once_a_character_exists", b.ShouldInstall(true, true, true));
        b.NoteInstall(true, true);
        check("install/installed", b.Installed() && b.InstallStateName() == "installed" && !b.TickNeeded()
              && !b.ShouldInstall(true, true, false) && !b.ShouldInstall(true, true, true));

        b.SetEnabled(false);
        check("install/switch_off_after_install_keeps_the_hooks", b.Installed() && b.InstallStateName() == "installed"
              && !b.Enabled());

        // A refused install turns the switch off and says so; the tick does not
        // try again, the command does.
        Bonus r;
        r.SetEnabled(true);
        r.NoteInstall(false, true);
        check("install/refused_turns_the_switch_off", r.InstallStateName() == "refused" && !r.Enabled() && !r.Installed()
              && !r.TickNeeded());
        r.SetEnabled(true);
        check("install/after_a_refusal_the_tick_does_not_retry_the_command_does",
              !r.ShouldInstall(true, true, true) && r.ShouldInstall(true, true, false) && !r.TickNeeded());
        r.NoteInstall(true, false);
        check("install/a_later_install_clears_the_refusal", r.InstallStateName() == "installed");

        // `mark1set bonus 1` with a character loaded installs at once.
        Bonus c;
        c.SetEnabled(true);
        check("install/command_with_a_character_installs_at_once", c.ShouldInstall(true, true, false));
    }

    // ---- the stat line -------------------------------------------------------------
    {
        const std::string fresh = StatLine(StatFields{});
        check("stat/fresh_line_begins_bonus_off", startsWith(fresh, "mark1set: bonus=off"), fresh);
        const std::string expected =
            "mark1set: bonus=off active=no install=not-armed set=none unreadable=0 reads=0 worn=-/-/-/-"
            " | automation presses=0/0/0/0 skip-empty=0/0/0/0 skip-aura=0/0/0/0 skip-toggle-on=0/0/0/0"
            " skip-settle=0/0/0/0 skip-no-state=0/0/0/0"
            " col-unreadable=0/0/0/0 orig-unreadable=0/0/0/0 write-failed=0/0/0/0 restore-failed=0/0/0/0"
            " gate-calls=0 gate-other=0 threw=0"
            " landed=0/0/0/0 skip-turn=0/0/0/0 yielded=0/0/0/0 cast-calls=0"
            " | jump granted=0 backed-off=0 refused-landing=0 refused-gate=0 refused-cursor=0 max=175(start)"
            " refused-unreadable=0 jumps=0 scaled=0 clamped=0 answered=0 excluded=0 rejected=0/0/0/0 unread=0/0/0/0/0/0"
            " grid-reads=0/0/0 | icons=0/0 art=off ground=0 ground-route=not-installed tip=0 tip-unread=0"
            " tip-armed=0 typeline=0 typeline-miss=0 tip-cached=0 typeline-hook=not-tried | speed=unavailable penalty=unavailable";
        check("stat/fresh_line_exact", fresh == expected, fresh);

        bool ok = true;
        for (const char* f : { " install=", " set=", " unreadable=", " presses=", " skip-empty=", " skip-aura=",
                               " granted=", " backed-off=", " refused-landing=", " refused-gate=", " refused-cursor=",
                               " max=", " icons=", " speed=", " penalty=" })
            if (!has(fresh, f)) ok = false;
        check("stat/named_fields", ok);

        StatFields f;
        f.columns[0].presses = 12;
        f.columns[1].skipEmpty = 3;
        f.columns[2].skipAura = 4;
        f.columns[3].skipToggleOn = 5;
        f.columns[3].skipSettle = 6;
        f.columns[0].skipNoState = 7;
        f.jump.granted = 2;
        f.jump.backedOff = 1;
        f.jump.refusedLanding = 3;
        f.jump.refusedGate = 4;
        f.jump.refusedCursor = 5;
        f.jump.max = 212.5;
        f.jump.maxSource = "cap";
        f.iconsLoaded = 8;
        f.iconsEmbedded = 8;
        f.speed = { true, 40 };
        f.penalty = { true, 0 };
        const std::string line = StatLine(f);
        ok = has(line, " presses=12/0/0/0") && has(line, " skip-empty=0/3/0/0") && has(line, " skip-aura=0/0/4/0")
            && has(line, " skip-toggle-on=0/0/0/5") && has(line, " skip-settle=0/0/0/6") && has(line, " skip-no-state=7/0/0/0")
            && has(line, " granted=2 backed-off=1 refused-landing=3 refused-gate=4 refused-cursor=5 max=212.5(cap)")
            && has(line, " icons=8/8") && has(line, " speed=40 penalty=0");
        check("stat/filled_fields", ok, line);

        // The set's part, filled by the bonus itself.
        Bonus b = installedOn();
        FakeWorn w;
        w.slots[1] = emptySlot();
        b.Active(0, w.fn());
        StatFields s;
        b.FillStat(s);
        const std::string partial = StatLine(s);
        check("stat/partial_set", startsWith(partial, "mark1set: bonus=on active=no install=installed set=partial(3) unreadable=0 reads=1 "),
              partial);

        Bonus full = installedOn();
        FakeWorn wf;
        full.Active(0, wf.fn());
        StatFields sf;
        full.FillStat(sf);
        check("stat/full_set", startsWith(StatLine(sf), "mark1set: bonus=on active=yes install=installed set=full unreadable=0 reads=1 "),
              StatLine(sf));

        // The line names the slot each piece was found in, `-` for one not found.
        Bonus gl = installedOn();
        FakeWorn wg;
        wg.slots[3] = emptySlot();
        wg.slots[5] = pieceIn(Piece::Gloves);
        gl.ReadNow(0, wg.fn());
        StatFields sg;
        gl.FillStat(sg);
        check("stat/worn_names_the_slot_each_piece_was_found_in",
              has(StatLine(sg), " set=full unreadable=0 reads=1 worn=0/1/2/5 | automation ") && field(partial, "worn") == "0/-/2/3",
              StatLine(sg));

        Bonus bad = installedOn();
        FakeWorn wb;
        wb.ok = false;
        bad.Active(0, wb.fn());
        StatFields sb;
        bad.FillStat(sb);
        check("stat/failed_read", has(StatLine(sb), " set=none unreadable=1 reads=1 "), StatLine(sb));

        Bonus waiting;
        waiting.SetEnabled(true);
        StatFields sw;
        waiting.FillStat(sw);
        check("stat/waiting_for_character", startsWith(StatLine(sw), "mark1set: bonus=on active=no install=waiting-for-character "),
              StatLine(sw));

        ok = NumberText(175) == "175" && NumberText(212.5) == "212.5" && NumberText(212.25) == "212.3"
            && NumberText(0) == "0" && NumberText(std::nan("")) == "?";
        check("stat/number_text", ok);
        check("stat/no_double_space", fresh.find("  ") == std::string::npos && line.find("  ") == std::string::npos);
    }

    // ---- failure_counts: the automation's failures and the jump's reads -------------
    {
        // Every name the line prints, each exactly once (a field printed twice,
        // or one whose name is another's, would let a check read the wrong one).
        const std::vector<std::string> names = {
            "bonus", "active", "install", "set", "unreadable", "reads", "worn",
            "presses", "skip-empty", "skip-aura", "skip-toggle-on", "skip-settle", "skip-no-state",
            "col-unreadable", "orig-unreadable", "write-failed", "restore-failed", "gate-calls", "gate-other", "threw",
            "landed", "skip-turn", "yielded", "cast-calls",
            "granted", "backed-off", "refused-landing", "refused-gate", "refused-cursor", "max",
            "refused-unreadable", "jumps", "scaled", "clamped", "answered", "excluded", "rejected", "unread", "grid-reads",
            "icons", "art", "ground", "ground-route", "tip", "tip-unread", "tip-armed", "typeline", "typeline-miss",
            "tip-cached", "typeline-hook", "speed", "penalty" };
        const std::string fresh = StatLine(StatFields{});
        bool ok = fieldNames(fresh) == names;
        check("failure_counts/every_field_once_in_order", ok, fresh);

        // A distinct value in every new field, each found under its own name.
        StatFields f;
        for (int i = 0; i < kColumnCount; ++i) {
            f.columns[i].unreadable = 21 + i;
            f.columns[i].originalUnreadable = 31 + i;
            f.columns[i].pressWriteFailed = 41 + i;
            f.columns[i].restoreFailed = 51 + i;
        }
        f.gateCalls = 61;
        f.gateOther = 62;
        f.threw = 63;
        f.jump.refusedUnreadable = 71;
        f.jump.jumps = 72;
        f.jump.scaled = 73;
        f.jump.clamped = 74;
        f.jump.answered = 75;
        f.jump.excluded = 76;
        f.jump.rejectedRoom = 81;
        f.jump.rejectedFamily = 82;
        f.jump.rejectedGrid = 83;
        f.jump.rejectedGate = 84;
        f.jump.unreadTakeoff = 91;
        f.jump.unreadCursor = 92;
        f.jump.unreadRoom = 93;
        f.jump.unreadFamily = 94;
        f.jump.unreadGrid = 95;
        f.jump.unreadGate = 96;
        f.jump.gridFree = 97;
        f.jump.gridBlocked = 98;
        f.jump.gridNoGrid = 99;
        f.artArmed = true;
        f.ground = 101;
        f.groundRoute = "table-only";
        const std::string line = StatLine(f);
        ok = field(line, "col-unreadable") == "21/22/23/24" && field(line, "orig-unreadable") == "31/32/33/34"
            && field(line, "write-failed") == "41/42/43/44" && field(line, "restore-failed") == "51/52/53/54"
            && field(line, "gate-calls") == "61" && field(line, "gate-other") == "62" && field(line, "threw") == "63";
        check("failure_counts/automation_fields_under_their_own_names", ok, line);

        ok = field(line, "refused-unreadable") == "71" && field(line, "jumps") == "72" && field(line, "scaled") == "73"
            && field(line, "clamped") == "74" && field(line, "answered") == "75" && field(line, "excluded") == "76"
            && field(line, "rejected") == "81/82/83/84" && field(line, "unread") == "91/92/93/94/95/96"
            && field(line, "grid-reads") == "97/98/99";
        check("failure_counts/jump_fields_under_their_own_names", ok, line);

        ok = field(line, "art") == "armed" && field(line, "ground") == "101" && field(line, "ground-route") == "table-only";
        check("failure_counts/icons_fields_under_their_own_names", ok, line);

        // The old fields keep their zeros: nothing new was folded into them.
        ok = field(line, "skip-no-state") == "0/0/0/0" && field(line, "unreadable") == "0"
            && field(line, "refused-landing") == "0" && field(line, "refused-gate") == "0"
            && field(line, "refused-cursor") == "0" && field(line, "presses") == "0/0/0/0";
        check("failure_counts/no_failure_folded_into_an_old_field", ok, line);
        check("failure_counts/same_names_filled", fieldNames(line) == names, line);

        // A column whose bar slot could not be read is not a toggle with no state.
        StatFields u;
        u.columns[2].unreadable = 9;
        const std::string col = StatLine(u);
        check("failure_counts/column_unreadable_never_in_skip_no_state",
              field(col, "col-unreadable") == "0/0/9/0" && field(col, "skip-no-state") == "0/0/0/0", col);

        // Grid reads by answer: free/blocked/nogrid, so a grid read that never
        // attaches (every cell NoGrid) is told apart from free cells.
        StatFields g;
        g.jump.gridFree = 5;
        g.jump.gridBlocked = 2;
        g.jump.gridNoGrid = 7;
        const std::string grid = StatLine(g);
        check("failure_counts/grid_reads_by_answer_free_blocked_nogrid",
              field(grid, "grid-reads") == "5/2/7" && field(grid, "unread") == "0/0/0/0/0/0"
                  && field(grid, "rejected") == "0/0/0/0", grid);

        // Each group's new fields come after that group's existing ones.
        ok = col.find(" reads=") < col.find(" worn=") && col.find(" worn=") < col.find(" | automation ")
            && col.find(" skip-no-state=") < col.find(" col-unreadable=") && col.find(" threw=") < col.find(" | jump ")
            && col.find(" max=") < col.find(" refused-unreadable=") && col.find(" unread=") < col.find(" grid-reads=") && col.find(" grid-reads=") < col.find(" | icons=")
            && col.find(" icons=") < col.find(" art=") && col.find(" ground-route=") < col.find(" | speed=");
        check("failure_counts/new_fields_after_each_groups_own", ok, col);
    }

    // ---- turn_fields: the turn rule's counters in the automation group -------------
    {
        // A distinct value per column and field, each under its own name, in
        // Q/E/R/Y order, after `threw=` and before the jump group.
        StatFields f;
        for (int i = 0; i < kColumnCount; ++i) {
            f.columns[i].landed = 11 + i;
            f.columns[i].skipTurn = 21 + i;
            f.columns[i].yielded = 31 + i;
        }
        f.castCalls = 41;
        f.threw = 5;
        const std::string line = StatLine(f);
        bool ok = field(line, "landed") == "11/12/13/14" && field(line, "skip-turn") == "21/22/23/24"
            && field(line, "yielded") == "31/32/33/34" && field(line, "cast-calls") == "41";
        check("turn_fields/each_under_its_own_name", ok, line);
        ok = has(line, " threw=5 landed=11/12/13/14 skip-turn=21/22/23/24 yielded=31/32/33/34 cast-calls=41 | jump ");
        check("turn_fields/after_threw_in_order", ok, line);
        // Nothing folded into the older automation fields.
        ok = field(line, "presses") == "0/0/0/0" && field(line, "skip-empty") == "0/0/0/0"
            && field(line, "gate-calls") == "0" && field(line, "gate-other") == "0";
        check("turn_fields/no_turn_count_folded_into_an_old_field", ok, line);
        // A fresh line reads every turn field as zero.
        const std::string fresh = StatLine(StatFields{});
        check("turn_fields/fresh_zero", field(fresh, "landed") == "0/0/0/0" && field(fresh, "skip-turn") == "0/0/0/0"
            && field(fresh, "yielded") == "0/0/0/0" && field(fresh, "cast-calls") == "0", fresh);
    }

    // ---- art_rule: the pieces' art follows the switch ---------------------------------
    {
        // baseline: a fresh session is not armed, so nothing is written and the
        // ground consumer is not wanted, whatever icons loaded.
        Bonus fresh;
        bool ok = !fresh.ArtArmed();
        for (int bag = 0; bag < 2; ++bag)
            for (int grd = 0; grd < 2; ++grd)
                for (const char* route : { "none", "both", "table-only" })
                    if (ArtWritesIcon(fresh.ArtArmed(), bag != 0, grd != 0, route)) ok = false;
        ok = ok && !ArtGroundInstallPending(fresh.ArtArmed(), 8, false)
            && !ArtWantsGroundInstall(fresh.ArtArmed(), 8, false, true, true);
        check("art_rule/baseline_fresh_session_not_armed_writes_and_installs_nothing", ok);

        StatFields sf;
        fresh.FillStat(sf);
        check("art_rule/baseline_fresh_stat_reads_art_off", field(StatLine(sf), "art") == "off", StatLine(sf));

        // The first piece dressed arms it for the session; the switch going on
        // and off does not disarm, nor does a refused install (which turns the
        // switch off).
        Bonus b;
        b.NotePieceDressed();
        const bool armedOn = b.ArtArmed();
        b.SetEnabled(true);
        b.SetEnabled(false);
        const bool stillArmed = b.ArtArmed();
        Bonus r;
        r.NotePieceDressed();
        r.SetEnabled(true);
        r.NoteInstall(false, false);
        check("art_rule/a_piece_arms_and_the_switch_does_not_disarm", armedOn && stillArmed && !b.Enabled()
              && r.ArtArmed() && !r.Enabled());
        StatFields sa;
        b.FillStat(sa);
        check("art_rule/armed_stat_reads_art_armed", field(StatLine(sa), "art") == "armed", StatLine(sa));

        // Armed, a write needs the bag icon and its ground copy both loaded, and
        // a shared detour not yet installed or installed on both routes. A
        // refused install (`none`) refuses the write.
        ok = ArtWritesIcon(true, true, true, "not-installed") && !ArtWritesIcon(true, true, true, "none")
            && ArtWritesIcon(true, true, true, "both")
            && !ArtWritesIcon(true, true, true, "table-only") && !ArtWritesIcon(true, true, true, "")
            && !ArtWritesIcon(true, true, true, "other")
            && !ArtWritesIcon(true, false, true, "both") && !ArtWritesIcon(true, true, false, "both")
            && !ArtWritesIcon(true, false, false, "both") && !ArtWritesIcon(false, true, true, "both");
        check("art_rule/a_write_needs_both_copies_and_no_known_other_route", ok);
        check("art_rule/route_names", GroundRouteAllowsArt("not-installed") && !GroundRouteAllowsArt("none")
              && GroundRouteAllowsArt("both")
              && !GroundRouteAllowsArt("table-only") && !GroundRouteAllowsArt("BOTH"));

        // RouteName() reads `none` both before the install attempt and after a
        // refused one; GroundRouteName separates the two, and only the refused
        // install fails closed. A fresh stat line names the not-yet-installed state.
        StatFields sd;
        ok = GroundRouteName(false, "none") == "not-installed" && GroundRouteName(true, "none") == "none"
            && GroundRouteName(true, "both") == "both"
            && ArtWritesIcon(true, true, true, GroundRouteName(false, "none"))
            && !ArtWritesIcon(true, true, true, GroundRouteName(true, "none"))
            && field(StatLine(sd), "ground-route") == "not-installed";
        check("art_rule/refused_install_fails_closed", ok, StatLine(sd));

        // The ground install: armed, an icon embedded, not tried, setup done and
        // a character resolved; never at character select. The switch is not a
        // parameter: once armed it is wanted whatever the switch now says.
        ok = ArtWantsGroundInstall(true, 8, false, true, true)
            && ArtWantsGroundInstall(true, 1, false, true, true)
            && !ArtWantsGroundInstall(false, 8, false, true, true)
            && !ArtWantsGroundInstall(true, 0, false, true, true)
            && !ArtWantsGroundInstall(true, 8, true, true, true)
            && !ArtWantsGroundInstall(true, 8, false, false, true)
            && !ArtWantsGroundInstall(true, 8, false, true, false);   // character select: no player
        check("art_rule/ground_install_wanted_only_when_armed_embedded_untried_and_in_game", ok);
        ok = ArtGroundInstallPending(true, 8, false) && !ArtGroundInstallPending(false, 8, false)
            && !ArtGroundInstallPending(true, 0, false) && !ArtGroundInstallPending(true, 8, true)
            && ArtGroundInstallPending(stillArmed, 8, false);
        check("art_rule/pending_is_the_tick_gate_before_any_game_call", ok);
    }

    // ---- the command -----------------------------------------------------------------
    {
        Command c;
        std::string reason;
        bool ok = ParseCommand(words(""), c, reason) && c.form == CommandForm::Stat
            && ParseCommand(words("stat"), c, reason) && c.form == CommandForm::Stat
            && ParseCommand(words("bonus 1"), c, reason) && c.form == CommandForm::Bonus && c.on
            && ParseCommand(words("bonus 0"), c, reason) && c.form == CommandForm::Bonus && !c.on;
        check("parse/accepted_forms", ok);

        const std::string usage = "mark1set: " + std::string(kUsage);
        ok = why("bonus") == usage && why("bonus 2") == usage && why("bonus on") == usage && why("stat 1") == usage
            && why("helmet 1") == usage && why("bonus 1 1") == usage;
        check("parse/refused_forms_print_the_usage", ok, why("bonus"));
        check("parse/usage_names_every_form", has(std::string(kUsage), "mark1set bonus 1|0") && has(std::string(kUsage), "mark1set stat"));

        Bonus b;
        b.SetEnabled(true);
        check("parse/bonus_reply_names_the_switch_and_the_install",
              BonusReply(b) == "mark1set: bonus=on install=waiting-for-character", BonusReply(b));
    }

    // ---- drops_arm: the drops switch arms the same art flag ------------------------
    {
        // `drops 1|0` parses as its own form; the refused forms still print the usage.
        Command c;
        std::string reason;
        bool ok = ParseCommand(words("drops 1"), c, reason) && c.form == CommandForm::Drops && c.on
            && ParseCommand(words("drops 0"), c, reason) && c.form == CommandForm::Drops && !c.on
            && ParseCommand(words("bonus 1"), c, reason) && c.form == CommandForm::Bonus;
        const std::string usage = "mark1set: " + std::string(kUsage);
        ok = ok && why("drops") == usage && why("drops 2") == usage && why("drops on") == usage
            && why("drops 1 1") == usage && has(std::string(kUsage), "mark1set drops 1|0");
        check("drops_arm/parse_drops_form", ok, why("drops"));

        // A drops switch-on arms the art for the session, as the set's does,
        // without turning the set's switch on or installing anything.
        Bonus d;
        d.NoteDropsSwitch(true);
        const bool armed = d.ArtArmed() && !d.Enabled() && d.InstallStateName() == "not-armed" && !d.TickNeeded()
            && !d.ShouldInstall(true, true, false);
        d.NoteDropsSwitch(false);
        check("drops_arm/drops_on_arms_the_art_and_leaves_the_set_off", armed && d.ArtArmed() && !d.Enabled());

        // baseline: a drops switch-off on a fresh session arms nothing.
        Bonus f;
        f.NoteDropsSwitch(false);
        check("drops_arm/baseline_drops_off_arms_nothing", !f.ArtArmed());

        // One flag: armed by a piece or the drops' switch, read by the art rule
        // and the line.
        Bonus e;
        e.NotePieceDressed();
        e.SetEnabled(false);
        e.NoteDropsSwitch(false);
        StatFields sd;
        d.FillStat(sd);
        check("drops_arm/one_flag_a_piece_or_the_drops", e.ArtArmed() && d.ArtArmed()
              && ArtGroundInstallPending(d.ArtArmed(), 8, false) && field(StatLine(sd), "art") == "armed", StatLine(sd));

        // The first stat line and the bonus reply stay byte for byte.
        StatFields fresh;
        Bonus g;
        g.SetEnabled(true);
        check("drops_arm/first_line_and_bonus_reply_unchanged",
              startsWith(StatLine(fresh), "mark1set: bonus=off active=no install=not-armed")
              && BonusReply(g) == "mark1set: bonus=on install=waiting-for-character", BonusReply(g));
    }

    // ---- art_baseline: the switch alone no longer arms the art (owner playtest, 2026-10-21) ----
    {
        // A fresh core: not armed, and nothing pending for the ground install.
        Bonus fresh;
        check("art_baseline/a_fresh_core_is_not_armed_and_nothing_is_pending",
              !fresh.ArtArmed() && !ArtGroundInstallPending(fresh.ArtArmed(), 8, false)
                  && !ArtWantsGroundInstall(fresh.ArtArmed(), 8, false, true, true));
        // Nor is the pieces' type line, embedded art or not.
        check("art_baseline/unarmed_the_type_line_install_is_not_pending",
              !TypeLineInstallPending(fresh.ArtArmed(), false) && !ArtInstallPending(fresh.ArtArmed(), 8, false, false)
                  && !ArtInstallPending(fresh.ArtArmed(), 0, false, false)
                  && !ArtWantsInstall(fresh.ArtArmed(), 0, false, false, true, true));
        // `mark1set bonus 1` alone leaves it unarmed, before and after its
        // install, and after a refused one.
        Bonus on;
        on.SetEnabled(true);
        const bool unarmedOn = !on.ArtArmed();
        on.NoteInstall(true, false);
        Bonus refused;
        refused.SetEnabled(true);
        refused.NoteInstall(false, false);
        StatFields f;
        on.FillStat(f);
        check("art_baseline/bonus_1_alone_leaves_it_unarmed",
              unarmedOn && !on.ArtArmed() && !refused.ArtArmed() && !ArtGroundInstallPending(on.ArtArmed(), 8, false)
                  && field(StatLine(f), "art") == "off" && field(StatLine(f), "bonus") == "on", StatLine(f));
    }

    // ---- art_target: a piece's dressing and the drops' switch arm it, sticky ----------------
    {
        Bonus piece;
        piece.NotePieceDressed();
        const bool armed = piece.ArtArmed();
        piece.SetEnabled(true);
        piece.SetEnabled(false);
        piece.NoteDropsSwitch(false);
        piece.NoteInstall(false, false);
        check("art_target/a_piece_dressed_arms_and_stays_armed", armed && piece.ArtArmed() && !piece.Enabled()
              && piece.InstallStateName() == "refused");
        // Arming by a piece leaves the set's switch and install alone.
        Bonus quiet;
        quiet.NotePieceDressed();
        check("art_target/a_piece_leaves_the_switch_and_install_alone",
              quiet.ArtArmed() && !quiet.Enabled() && quiet.InstallStateName() == "not-armed" && !quiet.TickNeeded()
                  && !quiet.ShouldInstall(true, true, false));
        Bonus drops;
        drops.NoteDropsSwitch(true);
        drops.NoteDropsSwitch(false);
        check("art_target/the_drops_switch_arms_and_stays_armed", drops.ArtArmed());
        // With the flag set the ground install is pending, and wanted once a
        // character exists; the switch is not asked.
        check("art_target/armed_the_ground_install_is_pending",
              ArtGroundInstallPending(piece.ArtArmed(), 8, false) && ArtGroundInstallPending(drops.ArtArmed(), 8, false)
                  && ArtWantsGroundInstall(quiet.ArtArmed(), 8, false, true, true)
                  && !ArtWantsGroundInstall(quiet.ArtArmed(), 8, false, true, false));
        // The pieces' type line goes in once armed and a character exists,
        // with or without embedded art (instrument-blindness round 0: it no
        // longer shares the icons' gate), and once.
        bool ok = TypeLineInstallPending(quiet.ArtArmed(), false) && !TypeLineInstallPending(quiet.ArtArmed(), true)
            && ArtInstallPending(quiet.ArtArmed(), 0, false, false)
            && ArtWantsInstall(quiet.ArtArmed(), 0, false, false, true, true)
            && !ArtWantsInstall(quiet.ArtArmed(), 0, false, false, true, false)   // character select: no player
            && !ArtWantsInstall(quiet.ArtArmed(), 0, false, false, false, true)
            && !ArtInstallPending(quiet.ArtArmed(), 0, false, true)
            && ArtInstallPending(quiet.ArtArmed(), 8, false, true)               // the ground consumer still waits
            && ArtInstallPending(quiet.ArtArmed(), 8, true, false)               // the type line still waits
            && !ArtInstallPending(quiet.ArtArmed(), 8, true, true);
        check("art_target/the_type_line_install_waits_for_no_embedded_art", ok);
        StatFields f;
        quiet.FillStat(f);
        check("art_target/the_line_reads_art_armed_with_the_switch_off",
              field(StatLine(f), "art") == "armed" && field(StatLine(f), "bonus") == "off", StatLine(f));
    }

    // ---- tooltip_baseline: what is not a piece gets no block, and the colours are the game's ----
    {
        // What the adapter recognises (IsPiece, as Mark1PieceOf): another
        // class, a seed outside 777500..777503, `j` not 0 are not pieces, so
        // they get no block and keep the game's type line.
        Piece p = Piece::Helmet;
        bool ok = !IsPiece(3, 777500, 0, 0, 0, p) && !IsPiece(4, 777499, 0, 0, 0, p) && !IsPiece(4, 777504, 0, 0, 0, p)
            && !IsPiece(4, 777503, 0, 0, 1, p);
        check("tooltip_baseline/not_a_piece_gets_no_block", ok);
        ok = TypeLineFor(false, Piece::Gloves, "Satanic Gloves") == "Satanic Gloves"
            && TypeLineFor(false, Piece::Helmet, "Satanic Set Helmet") == "Satanic Set Helmet"
            && TypeLineFor(false, Piece::Boots, "Heroic Boots") == "Heroic Boots";
        check("tooltip_baseline/not_a_piece_keeps_its_type_line", ok);
        // A piece keeps any other text the pass draws: its name, another
        // class's type line, the footer.
        ok = TypeLineFor(true, Piece::Gloves, "Mark 1 Gloves") == "Mark 1 Gloves"
            && TypeLineFor(true, Piece::Gloves, "Satanic Boots") == "Satanic Boots"
            && TypeLineFor(true, Piece::Gloves, "Satanic Set Gloves") == "Satanic Set Gloves"
            && TypeLineFor(true, Piece::Gloves, "Tier ") == "Tier ";
        check("tooltip_baseline/a_piece_keeps_every_other_text", ok);
        // The core's colours as compiled, for the runner to pin against
        // hs-game-sdk's curated item_info.json (tooltip.set_section and
        // tooltip.skill_grant, the game's own display).
        std::cout << "COLOURS bonus_active=" << kColourBonusActive << " bonus_inactive=" << kColourBonusInactive
                  << " set_name=" << kColourSetName << " piece_worn=" << kColourPieceWorn
                  << " piece_not_worn=" << kColourPieceNotWorn << " description=" << kColourDescription << std::endl;
    }

    // ---- tooltip_target: the block by what is worn, the switch and the read ----------------
    {
        const auto rowsText = [](const std::vector<TipRow>& rows) {
            std::string s;
            for (size_t i = 0; i < rows.size(); ++i) s += (i ? "|" : "") + rows[i].text;
            return s;
        };
        const auto colours = [](const std::vector<TipRow>& rows) {
            std::string s;
            for (const TipRow& r : rows) s += (s.empty() ? "" : "/") + std::to_string(r.colour);
            return s;
        };
        // The block since the 2026-10-22 playtest: one empty row, the set
        // bonus's eight lines in table order with Automation's description
        // under `+1 to Automation`, one empty row, the set's name and the four
        // pieces.
        const std::string description =
            "Your Q, E, R and Y skills cast themselves whenever they are ready, in that order. "
            "Your jump goes three times as far, over scenery.";
        const std::string text =
            "|+500 Defense|+35% to All Resistances|Replenish Life 25%|+1 to Automation|" + description
            + "|+5 to All Skills|+750 to Life|Physical Damage Taken Reduced by 20%|-50% Movement Speed||Mark 1"
              "|Mark 1 Helmet|Mark 1 Body Armor|Mark 1 Boots|Mark 1 Gloves";
        const std::string grey = "4210752", green = "65280", gold = "7844807", desc = "8421504";
        // The expected colours: the bonus lines lit at `lit` pieces (0: none
        // lit), and each piece green by `worn`.
        const auto expected = [&](int lit, const bool (&worn)[4]) {
            const int thresholds[8] = { 2, 3, 3, 4, 4, 4, 4, 4 };
            std::string s = grey;
            for (int i = 0; i < 8; ++i) {
                s += "/" + (lit >= thresholds[i] ? gold : grey);
                if (i == 3) s += "/" + desc;
            }
            s += "/" + grey + "/" + gold;
            for (bool w : worn) s += "/" + (w ? green : grey);
            return s;
        };
        const bool noneWorn[4] = { false, false, false, false };
        const bool allWorn[4] = { true, true, true, true };

        WornSlots none;
        for (auto& s : none) s = emptySlot();
        const std::vector<TipRow> noneRows = SetBlockRows(Evaluate(none), true, true, true);
        bool ok = rowsText(noneRows) == text && noneRows.size() == 16 && colours(noneRows) == expected(0, noneWorn);
        // Only the description is wrapped and drawn in the small font.
        for (size_t i = 0; i < noneRows.size(); ++i)
            if (noneRows[i].wrapped != (i == 5) || noneRows[i].smallFont != (i == 5)) ok = false;
        check("tooltip_target/none_worn_four_grey_names_and_a_grey_bonus", ok, colours(noneRows));

        WornSlots two = none;
        two[0] = pieceIn(Piece::Helmet);
        two[7] = pieceIn(Piece::Gloves);
        const std::vector<TipRow> twoRows = SetBlockRows(Evaluate(two), true, true, true);
        const bool helmetGloves[4] = { true, false, false, true };
        ok = rowsText(twoRows) == text && colours(twoRows) == expected(2, helmetGloves);
        check("tooltip_target/two_worn_those_two_green", ok, colours(twoRows));

        const std::vector<TipRow> fullOff = SetBlockRows(Evaluate(fullSet()), true, false, true);
        ok = rowsText(fullOff) == text && colours(fullOff) == expected(0, allWorn);
        check("tooltip_target/four_worn_switch_off_all_green_and_a_grey_bonus", ok, colours(fullOff));

        const std::vector<TipRow> fullOn = SetBlockRows(Evaluate(fullSet()), true, true, true);
        ok = rowsText(fullOn) == text && colours(fullOn) == expected(4, allWorn)
            && BlockBonusActive(Evaluate(fullSet()), true, true, true) && !BlockUnread(Evaluate(fullSet()), true);
        check("tooltip_target/four_worn_switch_on_a_gold_bonus", ok, colours(fullOn));

        // A read that failed as a whole: nothing green, every line grey, even
        // with a SetRead that claims the full set; counted unread.
        const std::vector<TipRow> failed = SetBlockRows(Evaluate(fullSet()), false, true, true);
        ok = rowsText(failed) == text && colours(failed) == expected(0, noneWorn)
            && BlockUnread(Evaluate(fullSet()), false) && !BlockBonusActive(Evaluate(fullSet()), false, true, true);
        check("tooltip_target/a_failed_read_gives_nothing_green", ok, colours(failed));

        // An unreadable slot can leave a piece unfound, never found: the
        // pieces read elsewhere stay green, three are worn, and the block
        // counts unread.
        WornSlots partly = fullSet();
        partly[3] = WornSlot{};   // the gloves' slot, unreadable
        const SetRead pr = Evaluate(partly);
        const std::vector<TipRow> partlyRows = SetBlockRows(pr, true, true, true);
        const bool noGloves[4] = { true, true, true, false };
        ok = colours(partlyRows) == expected(3, noGloves) && BlockUnread(pr, true);
        check("tooltip_target/an_unreadable_slot_leaves_that_piece_grey_and_counts_unread", ok, colours(partlyRows));

        // The bonus lines by pieces worn (the 2026-10-22 playtest): each line
        // lit while at least its threshold of distinct pieces is worn, the
        // switch on and the bonus installed. 0, 2, 3 and 4 worn; the switch
        // off; not installed; a piece worn twice counts once.
        WornSlots three = none;
        three[0] = pieceIn(Piece::Helmet);
        three[1] = pieceIn(Piece::Body);
        three[5] = pieceIn(Piece::Boots);
        const bool threeWorn[4] = { true, true, true, false };
        WornSlots twice = two;
        twice[2] = pieceIn(Piece::Helmet);   // the helmet a second time
        ok = colours(SetBlockRows(Evaluate(none), true, true, true)) == expected(0, noneWorn)
            && colours(SetBlockRows(Evaluate(two), true, true, true)) == expected(2, helmetGloves)
            && colours(SetBlockRows(Evaluate(three), true, true, true)) == expected(3, threeWorn)
            && colours(SetBlockRows(Evaluate(fullSet()), true, true, true)) == expected(4, allWorn)
            && colours(SetBlockRows(Evaluate(three), true, false, true)) == expected(0, threeWorn)
            && colours(SetBlockRows(Evaluate(three), true, true, false)) == expected(0, threeWorn)
            && colours(SetBlockRows(Evaluate(twice), true, true, true)) == expected(2, helmetGloves);
        check("tooltip_target/bonus_lines_lit_by_the_pieces_worn", ok, colours(SetBlockRows(Evaluate(three), true, true, true)));

        // `-50% Movement Speed` is a row of its own, the last bonus line, lit
        // with the 4-piece lines; the description no longer says it.
        ok = fullOn.size() == 16 && fullOn[9].text == "-50% Movement Speed" && fullOn[9].colour == kColourBonusActive
            && !fullOn[9].wrapped && !fullOn[9].smallFont
            && SetBlockRows(Evaluate(three), true, true, true)[9].colour == kColourBonusInactive
            && fullOn[5].text.find("half speed") == std::string::npos
            && std::string(kAutomationDescription) == description;
        check("tooltip_target/movement_line_is_its_own_row", ok, fullOn[9].text);

        // The piece's type line reads like the game's own set pieces.
        ok = TypeLineFor(true, Piece::Gloves, "Satanic Gloves") == "Satanic Set Gloves"
            && TypeLineFor(true, Piece::Helmet, "Satanic Helmet") == "Satanic Set Helmet"
            && TypeLineFor(true, Piece::Body, "Satanic Body Armor") == "Satanic Set Body Armor"
            && TypeLineFor(true, Piece::Boots, "Satanic Boots") == "Satanic Set Boots"
            && TypeLine(Piece::Gloves) == "Satanic Set Gloves";
        check("tooltip_target/a_pieces_type_line_reads_satanic_set_gloves", ok, TypeLine(Piece::Gloves));

        // The fresh stat line carries the block's two counters, after ground-route=.
        const std::string fresh = StatLine(StatFields{});
        StatFields counted;
        counted.tip = 7;
        counted.tipUnread = 2;
        const std::string line = StatLine(counted);
        ok = has(fresh, " ground-route=not-installed tip=0 tip-unread=0 ") && field(line, "tip") == "7"
            && field(line, "tip-unread") == "2";
        check("tooltip_target/the_line_carries_tip_and_tip_unread", ok, line);

        // The on-screen passes armed for a piece, and of those the ones whose
        // type line was swapped and the ones that ended with no swap, after
        // tip-unread= (instrument-blindness round 0: the block's count alone
        // cannot tell a pass that never ran from one whose block never anchored,
        // and nothing counted the type line).
        StatFields passes;
        passes.tip = 3;
        passes.tipUnread = 1;
        passes.tipArmed = 5;
        passes.typeLine = 4;
        passes.typeLineMiss = 1;
        const std::string counted2 = StatLine(passes);
        ok = has(fresh, " tip=0 tip-unread=0 tip-armed=0 typeline=0 typeline-miss=0 ")
            && field(counted2, "tip") == "3" && field(counted2, "tip-unread") == "1" && field(counted2, "tip-armed") == "5"
            && field(counted2, "typeline") == "4" && field(counted2, "typeline-miss") == "1"
            && counted2.find(" tip-unread=") < counted2.find(" tip-armed=")
            && counted2.find(" tip-armed=") < counted2.find(" typeline=")
            && counted2.find(" typeline=") < counted2.find(" typeline-miss=")
            && counted2.find(" typeline-miss=") < counted2.find(" | speed=");
        check("tooltip_target/the_line_carries_tip_armed_typeline_and_typeline_miss", ok, counted2);

        // Live 1 (replan 1): the on-screen call runs on every frame a tooltip
        // shows, and the game draws the text on a hover's first frame only.
        // Which field one call raises, from armed for a piece, the stat calls
        // during it and the type-line swaps during it.
        const auto counts = [](bool armed, uint64_t stats, uint64_t swaps) {
            const TipCallCounts c = TipCallCountsOf(armed, stats, swaps);
            return std::string(c.tipArmed ? "A" : "-") + (c.typeLine ? "T" : "-") + (c.typeLineMiss ? "M" : "-")
                + (c.tipCached ? "C" : "-");
        };
        ok = counts(true, 1, 1) == "AT--" && counts(true, 14, 2) == "AT--";
        check("tooltip_target/a_drawn_call_that_swapped_raises_tip_armed_and_typeline", ok, counts(true, 1, 1));
        ok = counts(true, 3, 0) == "A-M-";
        check("tooltip_target/a_drawn_call_with_no_swap_raises_tip_armed_and_typeline_miss", ok, counts(true, 3, 0));
        ok = counts(true, 0, 1) == "AT--";
        check("tooltip_target/a_swap_with_no_stat_call_is_a_drawn_call", ok, counts(true, 0, 1));
        ok = counts(true, 0, 0) == "---C";
        check("tooltip_target/a_call_that_drew_nothing_is_cached", ok, counts(true, 0, 0));
        ok = counts(false, 0, 0) == "----" && counts(false, 5, 0) == "----" && counts(false, 0, 1) == "----"
            && counts(false, 5, 1) == "----";
        check("tooltip_target/a_call_not_armed_for_a_piece_raises_nothing", ok, counts(false, 5, 1));
        // Over a hover of 1 drawn frame and 200 cached ones, typeline +
        // typeline-miss equals tip-armed, and only tip-cached grows at rest.
        StatFields hover;
        const auto apply = [&hover](const TipCallCounts& c) {
            if (c.tipArmed) ++hover.tipArmed;
            if (c.typeLine) ++hover.typeLine;
            if (c.typeLineMiss) ++hover.typeLineMiss;
            if (c.tipCached) ++hover.tipCached;
        };
        apply(TipCallCountsOf(true, 2, 1));
        for (int i = 0; i < 200; ++i) apply(TipCallCountsOf(true, 0, 0));
        ok = hover.tipArmed == 1 && hover.typeLine == 1 && hover.typeLineMiss == 0 && hover.tipCached == 200
            && hover.typeLine + hover.typeLineMiss == hover.tipArmed;
        check("tooltip_target/a_hover_and_its_rest_count_one_drawn_and_the_rest_cached", ok, StatLine(hover));

        // `typeline-hook=`: not-tried until the install ran, then in or failed.
        ok = TypeLineHookName(false, false) == "not-tried" && TypeLineHookName(false, true) == "not-tried"
            && TypeLineHookName(true, true) == "in" && TypeLineHookName(true, false) == "failed"
            && StatFields{}.typeLineHook == "not-tried";
        check("tooltip_target/typeline_hook_names_not_tried_in_and_failed", ok, std::string(TypeLineHookName(true, false)));

        // The two new fields, appended after typeline-miss=, before ` | speed=`.
        StatFields cached;
        cached.tipArmed = 2;
        cached.typeLine = 2;
        cached.tipCached = 767;
        cached.typeLineHook = std::string(TypeLineHookName(true, true));
        const std::string counted3 = StatLine(cached);
        ok = has(fresh, " typeline-miss=0 tip-cached=0 typeline-hook=not-tried | speed=")
            && field(counted3, "tip-cached") == "767" && field(counted3, "typeline-hook") == "in"
            && field(counted3, "tip-armed") == "2" && field(counted3, "typeline") == "2"
            && counted3.find(" typeline-miss=") < counted3.find(" tip-cached=")
            && counted3.find(" tip-cached=") < counted3.find(" typeline-hook=")
            && counted3.find(" typeline-hook=") < counted3.find(" | speed=");
        check("tooltip_target/the_line_carries_tip_cached_and_typeline_hook", ok, counted3);

        // The bonus line asks the install too (round 1's NON-BLOCKING finding):
        // lit only with all four worn, the switch on and the bonus installed.
        // The Automation line (row 4) stands for the 4-piece lines.
        const auto bonusColour = [&](bool switchOn, bool installed) {
            return std::to_string(SetBlockRows(Evaluate(fullSet()), true, switchOn, installed)[4].colour);
        };
        ok = bonusColour(true, false) == grey && bonusColour(true, true) == gold && bonusColour(false, true) == grey
            && bonusColour(false, false) == grey
            && !BlockBonusActive(Evaluate(fullSet()), true, true, false)
            && BlockBonusActive(Evaluate(fullSet()), true, true, true)
            && !BlockBonusActive(Evaluate(fullSet()), true, false, true);
        check("tooltip_target/the_bonus_line_needs_the_switch_and_the_install", ok,
              bonusColour(true, false) + "/" + bonusColour(true, true) + "/" + bonusColour(false, true));
    }

    // ---- tooltip_target: where a piece's block anchors ---------------------------------
    {
        // A pass as the game makes it (RDM § 16.6): one DrawInventoryStatsNew
        // call per stat line it knows, in a fixed order, present or not. Of
        // Mark 1's keys the order is 29, 201, 48, 51, 52, 170, 59, 173, with
        // other ids between and after (Item Truth's captures). Each call is
        // (stat id, format); `has` the keys the item carries.
        struct Call { int id; double format; };
        const std::vector<Call> pass = {
            { 5, 0 }, { 29, 2 }, { 30, 2 }, { 201, 3 }, { 48, 3 }, { 49, 3 }, { 51, 3 }, { 52, 3 }, { 53, 2 },
            { 170, 2 }, { 171, 2 }, { 59, 3 }, { 60, 3 }, { 173, 2 }, { 174, 2 }, { 175, 3 }, { 300, 2 },
        };
        // The index of the call the block anchors at, -1 for none.
        const auto anchorAt = [&](Piece p, const std::vector<int>& has) {
            BlockAnchor a = BlockAnchor::For(p);
            for (size_t i = 0; i < pass.size(); ++i) {
                bool present = false;
                for (int k : has) if (k == pass[i].id) present = true;
                if (a.AnchorsHere(pass[i].id, a.Waits(pass[i].id) && present, pass[i].format)) return (int)i;
            }
            return -1;
        };
        // The pieces' own stat-line keys: everything StatsFor writes but the
        // header's 154 and the sockets row's 20.
        const BlockAnchor helmet = BlockAnchor::For(Piece::Helmet);
        bool ok = helmet.KeyCount() == 4 && helmet.Key(0) == 29 && helmet.Key(1) == 173 && helmet.Key(2) == 201 && helmet.Key(3) == 48
            && BlockAnchor::For(Piece::Body).KeyCount() == 4 && BlockAnchor::For(Piece::Boots).KeyCount() == 3
            && BlockAnchor::For(Piece::Gloves).KeyCount() == 3;
        // Helmet (29, 173, 201, 48): its last line is 173, at index 13, so the
        // block anchors at the next row call, index 14 - not at 29, its first.
        const int helmetAt = anchorAt(Piece::Helmet, { 154, 29, 173, 201, 48, 20 });
        // Gloves (29, 201, 51): the last is 51 (index 6), so index 7.
        const int glovesAt = anchorAt(Piece::Gloves, { 154, 29, 201, 51 });
        // Body (29, 52, 201, 170): the last is 170 (index 9), so index 10.
        const int bodyAt = anchorAt(Piece::Body, { 154, 29, 52, 201, 170, 20 });
        // Boots (29, 59, 173): the last is 173, so index 14.
        const int bootsAt = anchorAt(Piece::Boots, { 154, 29, 59, 173 });
        // A key the item does not carry draws no line: the block still waits
        // for its call and anchors there, after every line the piece drew.
        const int missingAt = anchorAt(Piece::Gloves, { 154, 29, 201 });
        // The old rule (ForgedTooltipRows.hpp's first present stat) would
        // have anchored at 29, index 1, between the header and the stats.
        ok = ok && helmetAt == 14 && glovesAt == 7 && bodyAt == 10 && bootsAt == 14 && missingAt == 6;
        check("tooltip_target/block_anchors_after_the_pieces_own_lines", ok,
              std::to_string(helmetAt) + "/" + std::to_string(glovesAt) + "/" + std::to_string(bodyAt) + "/"
                  + std::to_string(bootsAt) + "/" + std::to_string(missingAt));
        // A format that draws no stat row (0, 5, 6) is never the anchor.
        BlockAnchor g = BlockAnchor::For(Piece::Gloves);
        ok = !g.AnchorsHere(29, true, 2) && !g.AnchorsHere(201, true, 3) && !g.AnchorsHere(51, true, 3)
            && !g.AnchorsHere(700, false, 6) && !g.AnchorsHere(701, false, 0) && g.AnchorsHere(702, false, 2);
        check("tooltip_target/block_anchors_only_at_a_row_call", ok);
    }

    // ---- seed_codec: the pieces' roll bands (Mark1Rolls.hpp) ----------------------------
    {
        bool ok = true;
        for (const PieceRow& r : kPieces) {
            const auto rp = static_cast<ForgePact::Mark1Rolls::Piece>(static_cast<int>(r.piece));
            for (long long seed : { ForgePact::Mark1Rolls::LowSeed(rp), ForgePact::Mark1Rolls::TopSeed(rp), r.legacySeed }) {
                Piece p = Piece::Gloves;
                ForgePact::Mark1Rolls::Rolled rolled;
                if (!DecodePiece(r.itemType, static_cast<double>(seed), r.baseB, 0, 0, p, rolled) || p != r.piece
                    || rolled.legacy != (seed == r.legacySeed)) ok = false;
            }
        }
        check("seed_codec/band_and_legacy_seeds_are_pieces", ok);
        Piece p = Piece::Helmet;
        ok = !IsPiece(0, static_cast<double>(ForgePact::Mark1Rolls::LowSeed(ForgePact::Mark1Rolls::Piece::Boots)), 0, 0, 0, p)
            && !IsPiece(2, static_cast<double>(ForgePact::Mark1Rolls::TopSeed(ForgePact::Mark1Rolls::Piece::Boots) + 1), 0, 0, 0, p)
            && !IsPiece(0, 999981452.0, 0, 0, 0, p)
            && !IsPiece(2, static_cast<double>(ForgePact::Mark1Rolls::LowSeed(ForgePact::Mark1Rolls::Piece::Boots)), 0, 0, 1, p)
            && SeedsDistinctInBand();
        check("seed_codec/another_bands_seed_or_a_natural_seed_is_no_piece", ok);
    }

    // ---- asker: whose dispatcher calls the bonus and the cost answer (AskerOf) ----------------
    // The game's indices for the objects, as asset_get_index would resolve
    // them: the SDK's own enum (the plugin resolves by name at install).
    {
        using HeroSiege::Objects::GameObject;
        const auto known = [](GameObject o) { return ObjectIndex{ true, static_cast<int>(o) }; };
        const ObjectIndex unread{};
        const ObjectIndex unresolved[kTalentScreenObjectCount] = {};
        ObjectIndex resolved[kTalentScreenObjectCount];
        for (int i = 0; i < kTalentScreenObjectCount; ++i) resolved[i] = known(kTalentScreenObjects[i]);
        const GameObject outside[] = { GameObject::Mercenary_obj, GameObject::Player_obj, GameObject::UI_Button_Open_Mercenary_obj };

        // The list is the four talent-screen objects, in order, and nothing else.
        bool ok = kTalentScreenObjects[0] == GameObject::UI_Button_Talent_Player_obj
            && kTalentScreenObjects[1] == GameObject::UI_Talent_Screen_Attribute_obj
            && kTalentScreenObjects[2] == GameObject::UI_Talent_Screen_Allocate_obj
            && kTalentScreenObjects[3] == GameObject::Player_Talent_Tooltip_obj;
        for (GameObject o : kTalentScreenObjects) ok = ok && o != GameObject::Mercenary_obj;
        check("asker_target/the_list_is_the_four_talent_screen_objects", ok);

        // Baseline, what shipped before: the local player answered, whatever
        // its object read and whatever resolved.
        ok = AskerOf(true, unread, unresolved) == Asker::LocalPlayer && AskerOf(true, known(GameObject::Player_obj), unresolved) == Asker::LocalPlayer
            && AskerOf(true, unread, resolved) == Asker::LocalPlayer;
        check("asker_baseline/local_player_answered", ok);
        // With every talent-screen name unresolved, every other self is left,
        // the four objects' own instances and Mercenary_obj included.
        ok = true;
        for (GameObject o : kTalentScreenObjects) ok = ok && AskerOf(false, known(o), unresolved) == Asker::Left;
        for (GameObject o : outside) ok = ok && AskerOf(false, known(o), unresolved) == Asker::Left;
        check("asker_baseline/names_unresolved_every_other_self_left", ok);

        // Target: an instance of each of the four, resolved, answered as the
        // talent screen; one resolved alone answers only its own.
        ok = true;
        for (GameObject o : kTalentScreenObjects) ok = ok && AskerOf(false, known(o), resolved) == Asker::TalentScreen;
        ObjectIndex one[kTalentScreenObjectCount] = {};
        one[2] = known(kTalentScreenObjects[2]);
        ok = ok && AskerOf(false, known(kTalentScreenObjects[2]), one) == Asker::TalentScreen
            && AskerOf(false, known(kTalentScreenObjects[0]), one) == Asker::Left;
        check("asker_target/each_talent_screen_object_answered", ok);

        // Negative control: the mercenary, an object outside the list, an
        // unread object, and an unread object against unresolved names, all
        // left; a stale `index` behind `known == false` never matches.
        ok = AskerOf(false, known(GameObject::Mercenary_obj), resolved) == Asker::Left;
        check("asker_negative/mercenary_left", ok);
        ok = AskerOf(false, known(GameObject::Player_obj), resolved) == Asker::Left
            && AskerOf(false, known(GameObject::UI_Button_Open_Mercenary_obj), resolved) == Asker::Left;
        check("asker_negative/object_outside_the_list_left", ok);
        ok = AskerOf(false, unread, resolved) == Asker::Left
            && AskerOf(false, ObjectIndex{ false, static_cast<int>(kTalentScreenObjects[0]) }, resolved) == Asker::Left
            && AskerOf(false, ObjectIndex{ true, -1 }, resolved) == Asker::Left;
        check("asker_negative/unread_object_left", ok);
        ObjectIndex stale[kTalentScreenObjectCount];
        for (int i = 0; i < kTalentScreenObjectCount; ++i) stale[i] = ObjectIndex{ false, -1 };
        ObjectIndex staleEqual[kTalentScreenObjectCount];
        for (int i = 0; i < kTalentScreenObjectCount; ++i) staleEqual[i] = ObjectIndex{ false, static_cast<int>(kTalentScreenObjects[i]) };
        ok = AskerOf(false, unread, unresolved) == Asker::Left && AskerOf(false, ObjectIndex{ false, -1 }, stale) == Asker::Left
            && AskerOf(false, known(kTalentScreenObjects[1]), staleEqual) == Asker::Left
            && AskerOf(false, ObjectIndex{ false, static_cast<int>(kTalentScreenObjects[1]) }, staleEqual) == Asker::Left;
        check("asker_negative/unread_against_unresolved_never_matches", ok);
    }

    // ---- asker page: the talent screen answered only on the player's own page (TalentPageOf) ----
    // Live 3 (merc-page): the Mercenary tab asks with the same four objects
    // as the Player tab, so the page is told apart by which screen object has
    // an instance: UI_Talent_Screen_obj (the player's) or
    // UI_Mercenary_Talents_obj (the mercenary's). The indices are the SDK
    // enum's, as asset_get_index would resolve them.
    {
        using HeroSiege::Objects::GameObject;
        const auto known = [](GameObject o) { return ObjectIndex{ true, static_cast<int>(o) }; };
        const auto count = [](long n) { return InstanceCount{ true, n }; };
        ObjectIndex screen[kTalentScreenObjectCount];
        for (int i = 0; i < kTalentScreenObjectCount; ++i) screen[i] = known(kTalentScreenObjects[i]);
        const ObjectIndex pages[kTalentPageObjectCount] = { known(kTalentPageObjects[0]), known(kTalentPageObjects[1]) };
        const InstanceCount playerPage[kTalentPageObjectCount] = { count(1), count(0) };
        const InstanceCount mercenaryPage[kTalentPageObjectCount] = { count(0), count(1) };
        const InstanceCount bothPages[kTalentPageObjectCount] = { count(1), count(1) };
        const InstanceCount noPage[kTalentPageObjectCount] = { count(0), count(0) };
        // Whether a talent-screen object's call is answered under a page reading.
        const auto screenAnswered = [&](GameObject o, TalentPage page) {
            return AnswersAsker(AskerOf(false, known(o), screen), page);
        };

        // The two page objects are the SDK's, the player's first.
        bool ok = kTalentPageObjects[0] == GameObject::UI_Talent_Screen_obj
            && kTalentPageObjects[1] == GameObject::UI_Mercenary_Talents_obj;
        // Baseline: on the player's page (one UI_Talent_Screen_obj, no
        // mercenary screen) each of the four objects is answered, as before.
        const InstanceCount twoPlayerScreens[kTalentPageObjectCount] = { count(2), count(0) };
        ok = ok && TalentPageOf(pages, playerPage) == TalentPage::Player && TalentPageOf(pages, twoPlayerScreens) == TalentPage::Player;
        for (GameObject o : kTalentScreenObjects) ok = ok && screenAnswered(o, TalentPageOf(pages, playerPage));
        check("asker_page_baseline/each_talent_screen_object_answered_on_the_players_page", ok);
        // The local player's own calls never depend on the page.
        ok = true;
        for (TalentPage p : { TalentPage::Player, TalentPage::Other, TalentPage::Unread })
            ok = ok && AnswersAsker(AskerOf(true, known(GameObject::Player_obj), screen), p)
                && AnswersAsker(AskerOf(true, ObjectIndex{}, screen), p);
        check("asker_page_baseline/local_player_answered_whatever_the_page", ok);

        // Target: on the mercenary's page (its screen, no player screen) each
        // of the four is left as the game answered it.
        ok = TalentPageOf(pages, mercenaryPage) == TalentPage::Other;
        for (GameObject o : kTalentScreenObjects) ok = ok && !screenAnswered(o, TalentPageOf(pages, mercenaryPage));
        check("asker_page_target/each_talent_screen_object_left_on_the_mercenarys_page", ok);

        // Negative control: a count that did not read, a name that did not
        // resolve, both screens at once and neither screen all leave the four;
        // an object outside the list stays left on the player's page.
        ok = true;
        const InstanceCount unreadCounts[][kTalentPageObjectCount] = {
            { InstanceCount{}, count(0) }, { count(1), InstanceCount{} }, { InstanceCount{}, InstanceCount{} },
            { InstanceCount{ false, 1 }, count(0) }, { count(1), InstanceCount{ false, 0 } }, { count(-1), count(0) },
        };
        for (const auto& counts : unreadCounts) {
            ok = ok && TalentPageOf(pages, counts) == TalentPage::Unread;
            for (GameObject o : kTalentScreenObjects) ok = ok && !screenAnswered(o, TalentPageOf(pages, counts));
        }
        check("asker_page_negative/count_unread_left", ok);
        ok = true;
        const ObjectIndex unresolvedPages[][kTalentPageObjectCount] = {
            { ObjectIndex{}, known(kTalentPageObjects[1]) }, { known(kTalentPageObjects[0]), ObjectIndex{} },
            { ObjectIndex{}, ObjectIndex{} },
            { ObjectIndex{ false, static_cast<int>(kTalentPageObjects[0]) }, known(kTalentPageObjects[1]) },
            { known(kTalentPageObjects[0]), ObjectIndex{ true, -1 } },
        };
        for (const auto& names : unresolvedPages) {
            ok = ok && TalentPageOf(names, playerPage) == TalentPage::Unread;
            for (GameObject o : kTalentScreenObjects) ok = ok && !screenAnswered(o, TalentPageOf(names, playerPage));
        }
        check("asker_page_negative/name_unresolved_left", ok);
        ok = TalentPageOf(pages, bothPages) == TalentPage::Other && TalentPageOf(pages, noPage) == TalentPage::Other;
        for (GameObject o : kTalentScreenObjects)
            ok = ok && !screenAnswered(o, TalentPageOf(pages, bothPages)) && !screenAnswered(o, TalentPageOf(pages, noPage));
        ok = ok && !AnswersAsker(AskerOf(false, known(GameObject::Mercenary_obj), screen), TalentPage::Player)
            && !AnswersAsker(AskerOf(false, known(GameObject::UI_Button_Open_Mercenary_obj), screen), TalentPage::Player)
            && !AnswersAsker(Asker::Left, TalentPage::Player);
        check("asker_page_negative/both_pages_or_none_left", ok);
    }

    // ---- stat: the second line -------------------------------------------------------------
    {
        const std::string fresh = BonusStatLine(BonusStatFields{});
        const std::string expected =
            "mark1set bonus: pieces=0 lit=0/0/0 answered=0/0/0/0/0/0 not-number=0 gear-reads=0 gear-unread=0"
            " | auto talents=-/-/-/- free=-/-/-/- masked=0 held-seen=0 hold=4 yield=61 rest=24 mask=0,1,2,7,8,9,10,11,27,40"
            " route=proc procs=0 proc-fail=0 locked=0 cost=drain drain=1.00 drained=0 cast-base=0.86 fcr=- haste=- stat-fail=0 sheet=0"
            " sheet-left=0 sheet-unread=0 | tip desc-font=not-drawn grey=0 grey-box=0 | rolls dressed=0 legacy=0 drop-rolls=0";
        check("stat/second_line_fresh_exact", fresh == expected, fresh);
        BonusStatFields f;
        f.pieces = 3;
        f.lit[0] = f.lit[1] = true;
        f.answered[0] = 7;
        f.answered[5] = 2;
        f.talents[0] = 23;
        f.talents[2] = 71;
        f.free[0] = 1;
        f.free[2] = 0;
        f.route = "press";
        f.drain = 1.25;
        f.castBase = 0.5;
        f.haveFcr = true;
        f.fcr = 40;
        f.haveHaste = true;
        f.haste = 12.5;
        f.descFont = DescFont::Small;
        f.grey = 2;
        f.legacy = 4;
        f.sheet = 9;
        f.sheetLeft = 3;
        f.sheetUnread = 1;
        const std::string line = BonusStatLine(f);
        const bool ok = has(line, "pieces=3 lit=1/1/0 answered=7/0/0/0/0/2 ") && has(line, " talents=23/-/71/- free=1/-/0/- ")
            && has(line, " route=press ") && has(line, " drain=1.25 ") && has(line, " cast-base=0.50 ") && has(line, " fcr=40 haste=12.5 ")
            && has(line, " stat-fail=0 sheet=9 sheet-left=3 sheet-unread=1 | tip ")
            && has(line, " desc-font=small grey=2 ") && has(line, " legacy=4 ") && !has(line, "  ")
            && std::string(DescFontName(DescFont::Unresolved)) == "unresolved" && AnsweredIndex(163) == 1 && AnsweredIndex(64) == -1;
        check("stat/second_line_filled", ok, line);
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
