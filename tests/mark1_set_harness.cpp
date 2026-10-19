// Behavioral regression harness for the Mark 1 set's core
// (Mark1Set.hpp, ForgePact #166 phase 2).
//
// The Python runner injects the REAL ForgePact::Mark1Set header below. No game
// is touched: the four worn slots are a WornSlots array the scenarios fill,
// handed to the core through a read callback that counts its calls, so a
// baseline can assert that nothing was read.
//
// Baseline (labels baseline/*): a vanilla item, a piece in the wrong slot,
// three pieces, an unreadable slot, a failed read and the switch off all
// leave the bonus inactive; with the switch off nothing is read at all.
// Seed codec (seed_codec/*): the table's classes, slots, seeds and bases, the
// seeds distinct and inside ForgePact's built-in band without touching a band
// in use, and recognition (`b` and `c` ignored, `j` must be 0, a whole
// number). Targets: four matching pieces with the switch on make the bonus
// active (active/*); the install state's transitions (install/*); the
// refresh period (refresh/*); the dressing (dressing/*); the stat line
// (stat/*) and the command's forms (parse/*). failure_counts/*: every field
// the line prints is there once, in order, and each failure count reaches it
// under its own name. art_rule/*: the pieces' art is armed by the first
// switch-on of a session (baseline: a fresh one writes and installs nothing),
// and the write and the ground install follow the rule.
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
    return item(r.itemType, static_cast<double>(r.seed), r.baseB, 0, 0);
}

static WornSlot emptySlot()
{
    WornSlot s;
    s.read = SlotRead::Empty;
    return s;
}

static WornSlots fullSet()
{
    WornSlots w;
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
        std::cout << "TABLE " << r.key << " type=" << r.itemType << " slot=" << r.slot << " seed=" << r.seed
                  << " b=" << r.baseB << std::endl;

    // ---- seed_codec --------------------------------------------------------------
    {
        bool ok = kPieceCount == 4
            && kPieces[0].piece == Piece::Helmet && kPieces[0].itemType == 0 && kPieces[0].slot == 0
            && kPieces[1].piece == Piece::Body && kPieces[1].itemType == 1 && kPieces[1].slot == 1
            && kPieces[2].piece == Piece::Boots && kPieces[2].itemType == 2 && kPieces[2].slot == 2
            && kPieces[3].piece == Piece::Gloves && kPieces[3].itemType == 4 && kPieces[3].slot == 3;
        check("seed_codec/classes_and_worn_slots", ok);

        ok = kPieces[0].seed == 777500 && kPieces[1].seed == 777501 && kPieces[2].seed == 777502 && kPieces[3].seed == 777503;
        check("seed_codec/one_seed_per_piece", ok);

        ok = true;
        for (int i = 0; i < kPieceCount; ++i) {
            if (kPieces[i].seed < kBuiltInSeedLo || kPieces[i].seed > kBuiltInSeedHi) ok = false;
            for (int k = i + 1; k < kPieceCount; ++k) if (kPieces[i].seed == kPieces[k].seed) ok = false;
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
                if (r.seed >= b.lo && r.seed <= b.hi) ok = false;
        check("seed_codec/seeds_collide_with_no_band_in_use", ok);

        ok = kPieces[0].baseB == 7 && kPieces[1].baseB == 0 && kPieces[2].baseB == 0 && kPieces[3].baseB == 0;
        check("seed_codec/provisional_bases", ok);

        ok = true;
        for (const PieceRow& r : kPieces) {
            Piece p = Piece::Gloves;
            if (!IsPiece(r.itemType, static_cast<double>(r.seed), r.baseB, 0, 0, p) || p != r.piece) ok = false;
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
        check("dressing/heroic_rarity_and_mechanic", kRarity == 9 && kMechanic == "mark1");
        ok = kAffixRowCount >= 1 && kAffixRowCount <= 3 && kAffixRows[0] == "Mark 1";
        for (int i = 1; i < kAffixRowCount; ++i)
            if (kAffixRows[i].empty() || kAffixRows[i] == "Mark 1") ok = false;
        check("dressing/affix_rows_first_reads_mark_1", ok, "rows=" + std::to_string(kAffixRowCount));
    }

    // ---- the full-set read -------------------------------------------------------
    {
        SetRead r = Evaluate(fullSet());
        check("active/four_matching_pieces_are_full", r.full && r.matched == 4 && r.unreadable == 0 && SetText(r) == "full",
              SetText(r));

        WornSlots w = fullSet();
        w[2] = item(2, 123456, 0, 0, 0);
        r = Evaluate(w);
        check("baseline/a_vanilla_item_is_not_full", !r.full && r.matched == 3 && SetText(r) == "partial(3)", SetText(r));

        w = fullSet();
        w[3] = emptySlot();
        r = Evaluate(w);
        check("baseline/three_pieces_are_not_full", !r.full && r.matched == 3 && r.unreadable == 0 && SetText(r) == "partial(3)",
              SetText(r));

        // The gloves worn where the boots go, and the boots where the gloves go.
        w = fullSet();
        w[2] = pieceIn(Piece::Gloves);
        w[3] = pieceIn(Piece::Boots);
        r = Evaluate(w);
        check("baseline/a_piece_in_the_wrong_slot_does_not_count", !r.full && r.matched == 2 && SetText(r) == "partial(2)",
              SetText(r));

        // Four helmets: one counts, in its own slot.
        WornSlots helmets;
        for (auto& s : helmets) s = pieceIn(Piece::Helmet);
        r = Evaluate(helmets);
        check("baseline/the_same_piece_four_times_is_one", !r.full && r.matched == 1 && SetText(r) == "partial(1)", SetText(r));

        w = fullSet();
        w[1].read = SlotRead::Unreadable;
        r = Evaluate(w);
        check("baseline/an_unreadable_slot_is_not_full", !r.full && r.matched == 3 && r.unreadable == 1, SetText(r));

        WornSlots none;
        for (auto& s : none) s = emptySlot();
        r = Evaluate(none);
        check("baseline/no_piece_reads_none", !r.full && r.matched == 0 && SetText(r) == "none", SetText(r));
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

        Bonus wrong = installedOn();
        FakeWorn ww;
        ww.slots[0] = pieceIn(Piece::Body);
        ww.slots[1] = pieceIn(Piece::Helmet);
        check("baseline/a_piece_in_the_wrong_slot_switch_on_inactive", !wrong.Active(0, ww.fn()));

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
            "mark1set: bonus=off active=no install=not-armed set=none unreadable=0 reads=0"
            " | automation presses=0/0/0/0 skip-empty=0/0/0/0 skip-aura=0/0/0/0 skip-toggle-on=0/0/0/0"
            " skip-settle=0/0/0/0 skip-no-state=0/0/0/0"
            " col-unreadable=0/0/0/0 orig-unreadable=0/0/0/0 write-failed=0/0/0/0 restore-failed=0/0/0/0"
            " gate-calls=0 gate-other=0 threw=0"
            " | jump granted=0 backed-off=0 refused-landing=0 refused-gate=0 refused-cursor=0 max=175(start)"
            " refused-unreadable=0 jumps=0 scaled=0 clamped=0 answered=0 excluded=0 rejected=0/0/0/0 unread=0/0/0/0/0/0"
            " | icons=0/0 art=off ground=0 ground-route=not-installed | speed=unavailable penalty=unavailable";
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
            "bonus", "active", "install", "set", "unreadable", "reads",
            "presses", "skip-empty", "skip-aura", "skip-toggle-on", "skip-settle", "skip-no-state",
            "col-unreadable", "orig-unreadable", "write-failed", "restore-failed", "gate-calls", "gate-other", "threw",
            "granted", "backed-off", "refused-landing", "refused-gate", "refused-cursor", "max",
            "refused-unreadable", "jumps", "scaled", "clamped", "answered", "excluded", "rejected", "unread",
            "icons", "art", "ground", "ground-route", "speed", "penalty" };
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
            && field(line, "rejected") == "81/82/83/84" && field(line, "unread") == "91/92/93/94/95/96";
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

        // Each group's new fields come after that group's existing ones.
        ok = col.find(" skip-no-state=") < col.find(" col-unreadable=") && col.find(" threw=") < col.find(" | jump ")
            && col.find(" max=") < col.find(" refused-unreadable=") && col.find(" unread=") < col.find(" | icons=")
            && col.find(" icons=") < col.find(" art=") && col.find(" ground-route=") < col.find(" | speed=");
        check("failure_counts/new_fields_after_each_groups_own", ok, col);
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

        // The first switch-on arms it for the session; switching off does not
        // disarm, nor does a refused install (which turns the switch off).
        Bonus b;
        b.SetEnabled(true);
        const bool armedOn = b.ArtArmed();
        b.SetEnabled(false);
        const bool stillArmed = b.ArtArmed();
        Bonus r;
        r.SetEnabled(true);
        r.NoteInstall(false, false);
        check("art_rule/switch_on_arms_and_off_does_not_disarm", armedOn && stillArmed && !b.Enabled()
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

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
